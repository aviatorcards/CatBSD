/*
 * plist_lite - minimal XML property list reader implementation
 *
 * Recursive descent over the raw buffer. There is no DOM, no schema and
 * no writing: the parser walks tags, builds the value tree, and fails
 * with a line number the moment it sees something it doesn't understand.
 * Failing loudly matters more than tolerance here -- a job plist that
 * silently half-parses turns into a daemon started with the wrong
 * arguments.
 */

#include "plist_lite.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * memmem() is a GNU/BSD extension that FreeBSD and glibc have but that
 * isn't in POSIX, so the parser carries its own. The inputs here are
 * short needles in a file-sized haystack; naive scanning is the right
 * complexity for that.
 */
static const char *memmem_compat(const char *hay, size_t hay_len,
                                 const char *needle, size_t needle_len) {
  if (needle_len == 0 || hay_len < needle_len) {
    return NULL;
  }
  for (size_t i = 0; i + needle_len <= hay_len; i++) {
    if (memcmp(hay + i, needle, needle_len) == 0) {
      return hay + i;
    }
  }
  return NULL;
}

struct plist_value {
  int type;
  union {
    struct {
      char **keys;
      plist_value_t **values;
      size_t count;
      size_t cap;
    } dict;
    struct {
      plist_value_t **items;
      size_t count;
      size_t cap;
    } array;
    char *string;
    int64_t integer;
    double real;
    int boolean;
    struct {
      unsigned char *bytes;
      size_t len;
    } data;
  } u;
};

typedef struct {
  const char *p;
  const char *end;
  const char *start;
  char *errbuf;
  size_t errlen;
  int failed;
} parser_t;

/* ------------------------------------------------------------------ */

static void fail(parser_t *ps, const char *fmt, ...) {
  if (!ps->failed && ps->errbuf != NULL && ps->errlen > 0) {
    /* Line number, counted lazily: parse errors are rare, so walking the
     * buffer here is cheaper than tracking lines on every character. */
    int line = 1;
    const char *q;
    char detail[192];
    va_list ap;

    for (q = ps->start; q < ps->p && q < ps->end; q++) {
      if (*q == '\n') {
        line++;
      }
    }

    va_start(ap, fmt);
    vsnprintf(detail, sizeof(detail), fmt, ap);
    va_end(ap);

    snprintf(ps->errbuf, ps->errlen, "line %d: %s", line, detail);
  }
  ps->failed = 1;
}

static void skip_ws(parser_t *ps) {
  while (ps->p < ps->end && isspace((unsigned char)*ps->p)) {
    ps->p++;
  }
}

/*
 * Skip the prologue noise that surrounds a real plist: the XML
 * declaration, the DOCTYPE, and comments. None of it carries data, but
 * all of it appears in every file Apple's tools emit.
 */
static int skip_prologue_at(parser_t *ps) {
  if (ps->end - ps->p < 2 || ps->p[0] != '<') {
    return 0;
  }

  if (ps->p[1] == '?') {
    const char *close = memmem_compat(ps->p, (size_t)(ps->end - ps->p), "?>", 2);
    if (close == NULL) {
      fail(ps, "unterminated <? ... ?> declaration");
      return 1;
    }
    ps->p = close + 2;
    return 1;
  }

  if (ps->end - ps->p >= 4 && memcmp(ps->p, "<!--", 4) == 0) {
    const char *close =
        memmem_compat(ps->p + 4, (size_t)(ps->end - ps->p - 4), "-->", 3);
    if (close == NULL) {
      fail(ps, "unterminated comment");
      return 1;
    }
    ps->p = close + 3;
    return 1;
  }

  if (ps->p[1] == '!') {
    /* <!DOCTYPE ...>, possibly with an internal subset in [ ]. */
    const char *q = ps->p + 2;
    int depth = 0;
    while (q < ps->end) {
      if (*q == '[') {
        depth++;
      } else if (*q == ']') {
        depth--;
      } else if (*q == '>' && depth <= 0) {
        ps->p = q + 1;
        return 1;
      }
      q++;
    }
    fail(ps, "unterminated <! ... > declaration");
    return 1;
  }

  return 0;
}

static void skip_noise(parser_t *ps) {
  for (;;) {
    skip_ws(ps);
    if (ps->failed || !skip_prologue_at(ps)) {
      return;
    }
  }
}

typedef struct {
  char name[32];
  int closing;      /* </foo> */
  int self_closing; /* <foo/> */
} tag_t;

/* Read the next tag. Returns 0 on failure. */
static int read_tag(parser_t *ps, tag_t *tag) {
  const char *q;
  size_t n = 0;

  skip_noise(ps);
  if (ps->failed) {
    return 0;
  }
  if (ps->p >= ps->end || *ps->p != '<') {
    fail(ps, "expected a tag");
    return 0;
  }

  ps->p++;
  memset(tag, 0, sizeof(*tag));

  if (ps->p < ps->end && *ps->p == '/') {
    tag->closing = 1;
    ps->p++;
  }

  while (ps->p < ps->end && (isalnum((unsigned char)*ps->p) || *ps->p == '_')) {
    if (n + 1 < sizeof(tag->name)) {
      tag->name[n++] = (char)tolower((unsigned char)*ps->p);
    }
    ps->p++;
  }
  tag->name[n] = '\0';

  if (n == 0) {
    fail(ps, "malformed tag");
    return 0;
  }

  /* Skip attributes (plist uses only version=, which we ignore). */
  q = ps->p;
  while (q < ps->end && *q != '>') {
    if (*q == '/' && q + 1 < ps->end && q[1] == '>') {
      tag->self_closing = 1;
    }
    q++;
  }
  if (q >= ps->end) {
    fail(ps, "unterminated <%s> tag", tag->name);
    return 0;
  }
  ps->p = q + 1;
  return 1;
}

/* ------------------------------------------------------------------ */

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} strbuf_t;

static int sb_putc(strbuf_t *sb, char c) {
  if (sb->len + 2 > sb->cap) {
    size_t cap = sb->cap ? sb->cap * 2 : 64;
    char *bigger = realloc(sb->buf, cap);
    if (bigger == NULL) {
      return 0;
    }
    sb->buf = bigger;
    sb->cap = cap;
  }
  sb->buf[sb->len++] = c;
  sb->buf[sb->len] = '\0';
  return 1;
}

/* Encode a Unicode code point as UTF-8, so &#233; survives round trips. */
static int sb_put_codepoint(strbuf_t *sb, unsigned long cp) {
  if (cp < 0x80) {
    return sb_putc(sb, (char)cp);
  }
  if (cp < 0x800) {
    return sb_putc(sb, (char)(0xC0 | (cp >> 6))) &&
           sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
  }
  if (cp < 0x10000) {
    return sb_putc(sb, (char)(0xE0 | (cp >> 12))) &&
           sb_putc(sb, (char)(0x80 | ((cp >> 6) & 0x3F))) &&
           sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
  }
  return sb_putc(sb, (char)(0xF0 | (cp >> 18))) &&
         sb_putc(sb, (char)(0x80 | ((cp >> 12) & 0x3F))) &&
         sb_putc(sb, (char)(0x80 | ((cp >> 6) & 0x3F))) &&
         sb_putc(sb, (char)(0x80 | (cp & 0x3F)));
}

/*
 * Read character data up to the next '<', resolving entity references.
 * Returns a malloc'd string (never NULL unless allocation failed).
 */
static char *read_text(parser_t *ps) {
  strbuf_t sb = {NULL, 0, 0};

  if (!sb_putc(&sb, '\0')) {
    fail(ps, "out of memory");
    return NULL;
  }
  sb.len = 0;
  sb.buf[0] = '\0';

  while (ps->p < ps->end && *ps->p != '<') {
    if (*ps->p == '&') {
      const char *semi = memchr(ps->p, ';', (size_t)(ps->end - ps->p));
      size_t elen;

      if (semi == NULL) {
        fail(ps, "unterminated entity reference");
        free(sb.buf);
        return NULL;
      }
      elen = (size_t)(semi - ps->p - 1);

      if (elen == 2 && memcmp(ps->p + 1, "lt", 2) == 0) {
        sb_putc(&sb, '<');
      } else if (elen == 2 && memcmp(ps->p + 1, "gt", 2) == 0) {
        sb_putc(&sb, '>');
      } else if (elen == 3 && memcmp(ps->p + 1, "amp", 3) == 0) {
        sb_putc(&sb, '&');
      } else if (elen == 4 && memcmp(ps->p + 1, "quot", 4) == 0) {
        sb_putc(&sb, '"');
      } else if (elen == 4 && memcmp(ps->p + 1, "apos", 4) == 0) {
        sb_putc(&sb, '\'');
      } else if (elen >= 2 && ps->p[1] == '#') {
        unsigned long cp;
        char numbuf[16];
        size_t nlen = elen - 1;

        if (nlen >= sizeof(numbuf)) {
          fail(ps, "oversized numeric entity");
          free(sb.buf);
          return NULL;
        }
        memcpy(numbuf, ps->p + 2, nlen);
        numbuf[nlen] = '\0';
        cp = (numbuf[0] == 'x' || numbuf[0] == 'X')
                 ? strtoul(numbuf + 1, NULL, 16)
                 : strtoul(numbuf, NULL, 10);
        sb_put_codepoint(&sb, cp);
      } else {
        fail(ps, "unknown entity reference");
        free(sb.buf);
        return NULL;
      }
      ps->p = semi + 1;
      continue;
    }

    if (!sb_putc(&sb, *ps->p)) {
      fail(ps, "out of memory");
      free(sb.buf);
      return NULL;
    }
    ps->p++;
  }

  return sb.buf;
}

/* Expect </name> right where we are. */
static int expect_close(parser_t *ps, const char *name) {
  tag_t tag;

  if (!read_tag(ps, &tag)) {
    return 0;
  }
  if (!tag.closing || strcmp(tag.name, name) != 0) {
    fail(ps, "expected </%s>, got <%s%s>", name, tag.closing ? "/" : "",
         tag.name);
    return 0;
  }
  return 1;
}

/* ------------------------------------------------------------------ */

static plist_value_t *value_alloc(int type) {
  plist_value_t *v = calloc(1, sizeof(*v));
  if (v != NULL) {
    v->type = type;
  }
  return v;
}

void plist_free(plist_value_t *v) {
  size_t i;

  if (v == NULL) {
    return;
  }

  switch (v->type) {
  case PLIST_DICT:
    for (i = 0; i < v->u.dict.count; i++) {
      free(v->u.dict.keys[i]);
      plist_free(v->u.dict.values[i]);
    }
    free(v->u.dict.keys);
    free(v->u.dict.values);
    break;
  case PLIST_ARRAY:
    for (i = 0; i < v->u.array.count; i++) {
      plist_free(v->u.array.items[i]);
    }
    free(v->u.array.items);
    break;
  case PLIST_STRING:
    free(v->u.string);
    break;
  case PLIST_DATA:
    free(v->u.data.bytes);
    break;
  default:
    break;
  }

  free(v);
}

static int dict_append(plist_value_t *d, char *key, plist_value_t *val) {
  if (d->u.dict.count == d->u.dict.cap) {
    size_t cap = d->u.dict.cap ? d->u.dict.cap * 2 : 8;
    char **k = realloc(d->u.dict.keys, cap * sizeof(*k));
    plist_value_t **vs;

    if (k == NULL) {
      return 0;
    }
    d->u.dict.keys = k;
    vs = realloc(d->u.dict.values, cap * sizeof(*vs));
    if (vs == NULL) {
      return 0;
    }
    d->u.dict.values = vs;
    d->u.dict.cap = cap;
  }
  d->u.dict.keys[d->u.dict.count] = key;
  d->u.dict.values[d->u.dict.count] = val;
  d->u.dict.count++;
  return 1;
}

static int array_append(plist_value_t *a, plist_value_t *val) {
  if (a->u.array.count == a->u.array.cap) {
    size_t cap = a->u.array.cap ? a->u.array.cap * 2 : 8;
    plist_value_t **items = realloc(a->u.array.items, cap * sizeof(*items));
    if (items == NULL) {
      return 0;
    }
    a->u.array.items = items;
    a->u.array.cap = cap;
  }
  a->u.array.items[a->u.array.count++] = val;
  return 1;
}

/* base64, tolerant of embedded whitespace (plists wrap data at 76 cols). */
static int base64_decode(const char *in, unsigned char **out, size_t *out_len) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t in_len = strlen(in);
  unsigned char *buf = malloc(in_len / 4 * 3 + 4);
  size_t n = 0;
  uint32_t acc = 0;
  int bits = 0;

  if (buf == NULL) {
    return 0;
  }

  for (size_t i = 0; i < in_len; i++) {
    const char *pos;
    char c = in[i];

    if (isspace((unsigned char)c)) {
      continue;
    }
    if (c == '=') {
      break;
    }
    pos = memchr(alphabet, c, 64);
    if (pos == NULL) {
      free(buf);
      return 0;
    }
    acc = (acc << 6) | (uint32_t)(pos - alphabet);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      buf[n++] = (unsigned char)((acc >> bits) & 0xFF);
    }
  }

  *out = buf;
  *out_len = n;
  return 1;
}

static plist_value_t *parse_value(parser_t *ps);

static plist_value_t *parse_after_tag(parser_t *ps, const tag_t *tag) {
  plist_value_t *v;
  char *text;

  if (strcmp(tag->name, "true") == 0 || strcmp(tag->name, "false") == 0) {
    v = value_alloc(PLIST_BOOL);
    if (v == NULL) {
      fail(ps, "out of memory");
      return NULL;
    }
    v->u.boolean = (tag->name[0] == 't');
    if (!tag->self_closing && !expect_close(ps, tag->name)) {
      plist_free(v);
      return NULL;
    }
    return v;
  }

  if (strcmp(tag->name, "dict") == 0) {
    v = value_alloc(PLIST_DICT);
    if (v == NULL) {
      fail(ps, "out of memory");
      return NULL;
    }
    if (tag->self_closing) {
      return v;
    }
    for (;;) {
      tag_t next;
      char *key;
      plist_value_t *child;

      if (!read_tag(ps, &next)) {
        plist_free(v);
        return NULL;
      }
      if (next.closing && strcmp(next.name, "dict") == 0) {
        return v;
      }
      if (strcmp(next.name, "key") != 0 || next.closing) {
        fail(ps, "expected <key> inside <dict>, got <%s>", next.name);
        plist_free(v);
        return NULL;
      }

      key = read_text(ps);
      if (key == NULL || !expect_close(ps, "key")) {
        free(key);
        plist_free(v);
        return NULL;
      }

      child = parse_value(ps);
      if (child == NULL) {
        free(key);
        plist_free(v);
        return NULL;
      }
      if (!dict_append(v, key, child)) {
        fail(ps, "out of memory");
        free(key);
        plist_free(child);
        plist_free(v);
        return NULL;
      }
    }
  }

  if (strcmp(tag->name, "array") == 0) {
    v = value_alloc(PLIST_ARRAY);
    if (v == NULL) {
      fail(ps, "out of memory");
      return NULL;
    }
    if (tag->self_closing) {
      return v;
    }
    for (;;) {
      const char *save;
      tag_t next;
      plist_value_t *child;

      skip_noise(ps);
      save = ps->p;
      if (!read_tag(ps, &next)) {
        plist_free(v);
        return NULL;
      }
      if (next.closing && strcmp(next.name, "array") == 0) {
        return v;
      }
      /* Not the closing tag, so it opens an element: rewind and let
       * parse_value handle it uniformly. */
      ps->p = save;
      child = parse_value(ps);
      if (child == NULL) {
        plist_free(v);
        return NULL;
      }
      if (!array_append(v, child)) {
        fail(ps, "out of memory");
        plist_free(child);
        plist_free(v);
        return NULL;
      }
    }
  }

  if (strcmp(tag->name, "string") == 0) {
    v = value_alloc(PLIST_STRING);
    if (v == NULL) {
      fail(ps, "out of memory");
      return NULL;
    }
    if (tag->self_closing) {
      v->u.string = strdup("");
      return v;
    }
    text = read_text(ps);
    if (text == NULL || !expect_close(ps, "string")) {
      free(text);
      plist_free(v);
      return NULL;
    }
    v->u.string = text;
    return v;
  }

  if (strcmp(tag->name, "integer") == 0 || strcmp(tag->name, "real") == 0) {
    int is_int = (tag->name[0] == 'i');

    v = value_alloc(is_int ? PLIST_INTEGER : PLIST_REAL);
    if (v == NULL) {
      fail(ps, "out of memory");
      return NULL;
    }
    text = read_text(ps);
    if (text == NULL || !expect_close(ps, tag->name)) {
      free(text);
      plist_free(v);
      return NULL;
    }
    if (is_int) {
      v->u.integer = strtoll(text, NULL, 10);
    } else {
      v->u.real = strtod(text, NULL);
    }
    free(text);
    return v;
  }

  if (strcmp(tag->name, "data") == 0) {
    v = value_alloc(PLIST_DATA);
    if (v == NULL) {
      fail(ps, "out of memory");
      return NULL;
    }
    if (tag->self_closing) {
      v->u.data.bytes = malloc(1);
      v->u.data.len = 0;
      return v;
    }
    text = read_text(ps);
    if (text == NULL || !expect_close(ps, "data")) {
      free(text);
      plist_free(v);
      return NULL;
    }
    if (!base64_decode(text, &v->u.data.bytes, &v->u.data.len)) {
      fail(ps, "malformed base64 in <data>");
      free(text);
      plist_free(v);
      return NULL;
    }
    free(text);
    return v;
  }

  fail(ps, "unsupported element <%s>", tag->name);
  return NULL;
}

static plist_value_t *parse_value(parser_t *ps) {
  tag_t tag;

  if (!read_tag(ps, &tag)) {
    return NULL;
  }
  if (tag.closing) {
    fail(ps, "unexpected </%s>", tag.name);
    return NULL;
  }
  return parse_after_tag(ps, &tag);
}

plist_value_t *plist_parse_string(const char *xml, size_t len, char *errbuf,
                                  size_t errlen) {
  parser_t ps;
  plist_value_t *root;
  tag_t tag;

  if (errbuf != NULL && errlen > 0) {
    errbuf[0] = '\0';
  }
  if (xml == NULL) {
    if (errbuf != NULL && errlen > 0) {
      snprintf(errbuf, errlen, "no input");
    }
    return NULL;
  }

  ps.p = xml;
  ps.start = xml;
  ps.end = xml + len;
  ps.errbuf = errbuf;
  ps.errlen = errlen;
  ps.failed = 0;

  if (!read_tag(&ps, &tag)) {
    return NULL;
  }

  /*
   * The <plist> wrapper is conventional but not load-bearing: files
   * written by hand sometimes start straight at <dict>, and rejecting
   * those would be pedantry rather than safety.
   */
  if (strcmp(tag.name, "plist") == 0 && !tag.closing) {
    root = parse_value(&ps);
    if (root == NULL) {
      return NULL;
    }
    if (!expect_close(&ps, "plist")) {
      plist_free(root);
      return NULL;
    }
    return root;
  }

  return parse_after_tag(&ps, &tag);
}

plist_value_t *plist_parse_file(const char *path, char *errbuf, size_t errlen) {
  FILE *f;
  char *buf;
  long size;
  size_t got;
  plist_value_t *v;

  if (errbuf != NULL && errlen > 0) {
    errbuf[0] = '\0';
  }
  if (path == NULL) {
    if (errbuf != NULL && errlen > 0) {
      snprintf(errbuf, errlen, "no path given");
    }
    return NULL;
  }

  f = fopen(path, "rb");
  if (f == NULL) {
    if (errbuf != NULL && errlen > 0) {
      snprintf(errbuf, errlen, "cannot open %s: %s", path, strerror(errno));
    }
    return NULL;
  }

  if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
      fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    if (errbuf != NULL && errlen > 0) {
      snprintf(errbuf, errlen, "cannot size %s", path);
    }
    return NULL;
  }

  buf = malloc((size_t)size + 1);
  if (buf == NULL) {
    fclose(f);
    if (errbuf != NULL && errlen > 0) {
      snprintf(errbuf, errlen, "out of memory reading %s", path);
    }
    return NULL;
  }

  got = fread(buf, 1, (size_t)size, f);
  fclose(f);
  buf[got] = '\0';

  v = plist_parse_string(buf, got, errbuf, errlen);
  free(buf);
  return v;
}

/* ------------------------------------------------------------------ */
/* Accessors                                                           */
/* ------------------------------------------------------------------ */

int plist_type(const plist_value_t *v) {
  return (v != NULL) ? v->type : PLIST_INVALID;
}

const plist_value_t *plist_dict_get(const plist_value_t *d, const char *key) {
  size_t i;

  if (d == NULL || d->type != PLIST_DICT || key == NULL) {
    return NULL;
  }
  for (i = 0; i < d->u.dict.count; i++) {
    if (strcmp(d->u.dict.keys[i], key) == 0) {
      return d->u.dict.values[i];
    }
  }
  return NULL;
}

size_t plist_dict_count(const plist_value_t *d) {
  return (d != NULL && d->type == PLIST_DICT) ? d->u.dict.count : 0;
}

const char *plist_dict_key_at(const plist_value_t *d, size_t i) {
  if (d == NULL || d->type != PLIST_DICT || i >= d->u.dict.count) {
    return NULL;
  }
  return d->u.dict.keys[i];
}

const plist_value_t *plist_dict_value_at(const plist_value_t *d, size_t i) {
  if (d == NULL || d->type != PLIST_DICT || i >= d->u.dict.count) {
    return NULL;
  }
  return d->u.dict.values[i];
}

size_t plist_array_count(const plist_value_t *a) {
  return (a != NULL && a->type == PLIST_ARRAY) ? a->u.array.count : 0;
}

const plist_value_t *plist_array_get(const plist_value_t *a, size_t i) {
  if (a == NULL || a->type != PLIST_ARRAY || i >= a->u.array.count) {
    return NULL;
  }
  return a->u.array.items[i];
}

const char *plist_string_value(const plist_value_t *v, const char *fallback) {
  return (v != NULL && v->type == PLIST_STRING) ? v->u.string : fallback;
}

int64_t plist_integer_value(const plist_value_t *v, int64_t fallback) {
  if (v == NULL) {
    return fallback;
  }
  if (v->type == PLIST_INTEGER) {
    return v->u.integer;
  }
  if (v->type == PLIST_REAL) {
    return (int64_t)v->u.real;
  }
  return fallback;
}

double plist_real_value(const plist_value_t *v, double fallback) {
  if (v == NULL) {
    return fallback;
  }
  if (v->type == PLIST_REAL) {
    return v->u.real;
  }
  if (v->type == PLIST_INTEGER) {
    return (double)v->u.integer;
  }
  return fallback;
}

int plist_bool_value(const plist_value_t *v, int fallback) {
  return (v != NULL && v->type == PLIST_BOOL) ? v->u.boolean : fallback;
}

const void *plist_data_value(const plist_value_t *v, size_t *len_out) {
  if (v == NULL || v->type != PLIST_DATA) {
    return NULL;
  }
  if (len_out != NULL) {
    *len_out = v->u.data.len;
  }
  return v->u.data.bytes;
}

const char *plist_dict_get_string(const plist_value_t *d, const char *key,
                                  const char *fallback) {
  return plist_string_value(plist_dict_get(d, key), fallback);
}

int64_t plist_dict_get_integer(const plist_value_t *d, const char *key,
                               int64_t fallback) {
  return plist_integer_value(plist_dict_get(d, key), fallback);
}

int plist_dict_get_bool(const plist_value_t *d, const char *key, int fallback) {
  return plist_bool_value(plist_dict_get(d, key), fallback);
}
