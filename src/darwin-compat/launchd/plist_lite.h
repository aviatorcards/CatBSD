/*
 * plist_lite - minimal XML property list reader for CatBSD
 *
 * launchd job definitions are XML plists, so anything that supervises
 * jobs has to read them. The obvious answer is libxml2, but this library
 * is meant to end up in a base system where a hard dependency on a large
 * external parser is a real cost -- and job plists use a tiny corner of
 * the format. So this is a self-contained reader for that corner:
 *
 *   <dict>, <array>, <key>, <string>, <integer>, <real>,
 *   <true/>, <false/>, <data> (base64)
 *
 * Deliberately unsupported: <date>, binary plists, and the writing
 * direction. plutil-demo already links libxml2 for the general case; this
 * exists for the dependency-free read path.
 */

#ifndef _CATBSD_PLIST_LITE_H_
#define _CATBSD_PLIST_LITE_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLIST_INVALID 0
#define PLIST_DICT 1
#define PLIST_ARRAY 2
#define PLIST_STRING 3
#define PLIST_INTEGER 4
#define PLIST_REAL 5
#define PLIST_BOOL 6
#define PLIST_DATA 7

typedef struct plist_value plist_value_t;

/*
 * Parse a plist. On failure both return NULL and, if `errbuf` is
 * non-NULL, write a message naming the line the parse gave up on --
 * a bad job plist should say where it is bad, not just that it is.
 */
plist_value_t *plist_parse_string(const char *xml, size_t len, char *errbuf,
                                  size_t errlen);
plist_value_t *plist_parse_file(const char *path, char *errbuf, size_t errlen);

void plist_free(plist_value_t *value);

int plist_type(const plist_value_t *value);

/* Dictionaries. Lookup is by key; the ordered accessors exist for
 * callers that need to iterate (config dumps, diffing). */
const plist_value_t *plist_dict_get(const plist_value_t *dict,
                                    const char *key);
size_t plist_dict_count(const plist_value_t *dict);
const char *plist_dict_key_at(const plist_value_t *dict, size_t index);
const plist_value_t *plist_dict_value_at(const plist_value_t *dict,
                                         size_t index);

size_t plist_array_count(const plist_value_t *array);
const plist_value_t *plist_array_get(const plist_value_t *array, size_t index);

/* Scalar accessors return the fallback when the value is absent or of
 * another type, which keeps job-plist reading free of type ceremony. */
const char *plist_string_value(const plist_value_t *value,
                               const char *fallback);
int64_t plist_integer_value(const plist_value_t *value, int64_t fallback);
double plist_real_value(const plist_value_t *value, double fallback);
int plist_bool_value(const plist_value_t *value, int fallback);
const void *plist_data_value(const plist_value_t *value, size_t *len_out);

/* Convenience: look a key up in `dict` and read it as a scalar. */
const char *plist_dict_get_string(const plist_value_t *dict, const char *key,
                                  const char *fallback);
int64_t plist_dict_get_integer(const plist_value_t *dict, const char *key,
                               int64_t fallback);
int plist_dict_get_bool(const plist_value_t *dict, const char *key,
                        int fallback);

#ifdef __cplusplus
}
#endif

#endif /* _CATBSD_PLIST_LITE_H_ */
