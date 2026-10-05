/*
 * Test program for the CatBSD XPC compatibility shim
 *
 * Every test asserts on observable behaviour rather than return codes
 * alone: a shim that "succeeds" while losing bytes, dropping messages or
 * mismatching replies would pass a return-code-only suite.
 */

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "xpc_shim.h"

#define OK "  \xE2\x9C\x93 "

/*
 * Local monotonic clock rather than darwin_absolute_time(): the XPC shim
 * has no dependency on the Mach or syscall shims, and this test shouldn't
 * invent one just to time a timeout.
 */
static uint64_t mono_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Unique-ish service name so concurrent runs don't collide. */
static char g_service[64];

static void make_service_name(void) {
  snprintf(g_service, sizeof(g_service), "com.catbsd.test.%ld",
           (long)getpid());
}

/* ------------------------------------------------------------------ */

static void test_dictionary_types(void) {
  printf("Testing XPC dictionary value types...\n");

  catbsd_xpc_object_t d = catbsd_xpc_dictionary_create();
  assert(d != NULL);

  assert(catbsd_xpc_dictionary_set_bool(d, "flag", 1) == CATBSD_XPC_SUCCESS);
  assert(catbsd_xpc_dictionary_set_int64(d, "signed", -42) ==
         CATBSD_XPC_SUCCESS);
  assert(catbsd_xpc_dictionary_set_uint64(d, "unsigned", 18446744073709551615ULL) ==
         CATBSD_XPC_SUCCESS);
  assert(catbsd_xpc_dictionary_set_double(d, "ratio", 0.5) ==
         CATBSD_XPC_SUCCESS);
  assert(catbsd_xpc_dictionary_set_string(d, "label", "com.catbsd.meow") ==
         CATBSD_XPC_SUCCESS);
  assert(catbsd_xpc_dictionary_set_data(d, "blob", "\x00\x01\x02\xFF", 4) ==
         CATBSD_XPC_SUCCESS);

  assert(catbsd_xpc_dictionary_count(d) == 6);

  int b = 0;
  int64_t i = 0;
  uint64_t u = 0;
  double f = 0;
  size_t len = 0;

  assert(catbsd_xpc_dictionary_get_bool(d, "flag", &b) == CATBSD_XPC_SUCCESS &&
         b == 1);
  assert(catbsd_xpc_dictionary_get_int64(d, "signed", &i) ==
             CATBSD_XPC_SUCCESS &&
         i == -42);
  assert(catbsd_xpc_dictionary_get_uint64(d, "unsigned", &u) ==
             CATBSD_XPC_SUCCESS &&
         u == 18446744073709551615ULL);
  assert(catbsd_xpc_dictionary_get_double(d, "ratio", &f) ==
             CATBSD_XPC_SUCCESS &&
         f == 0.5);
  assert(strcmp(catbsd_xpc_dictionary_get_string(d, "label"),
                "com.catbsd.meow") == 0);

  const unsigned char *blob = catbsd_xpc_dictionary_get_data(d, "blob", &len);
  assert(blob != NULL && len == 4);
  /* Embedded NUL survives -- data is not secretly treated as a string. */
  assert(blob[0] == 0x00 && blob[1] == 0x01 && blob[3] == 0xFF);
  printf(OK "all six value types round-trip in memory, including an "
            "embedded NUL in data\n");

  /*
   * The strict-typing contract: a key that exists but holds another type
   * reads as absent rather than silently coercing. Darwin's XPC folds
   * "missing" and "zero" together; a supervisor deciding whether a job
   * set KeepAlive needs to tell those apart.
   */
  assert(catbsd_xpc_dictionary_get_int64(d, "label", &i) == CATBSD_XPC_EINVAL);
  assert(i == -42); /* out-param untouched on failure */
  assert(catbsd_xpc_dictionary_get_string(d, "signed") == NULL);
  assert(catbsd_xpc_dictionary_get_bool(d, "nope", &b) == CATBSD_XPC_EINVAL);
  printf(OK "type mismatches and missing keys fail instead of coercing\n");

  /* Overwrite changes both value and type in place. */
  assert(catbsd_xpc_dictionary_set_string(d, "signed", "now a string") ==
         CATBSD_XPC_SUCCESS);
  assert(catbsd_xpc_dictionary_count(d) == 6);
  assert(catbsd_xpc_dictionary_get_type(d, "signed") ==
         CATBSD_XPC_TYPE_STRING);
  printf(OK "overwriting a key replaces its type without growing the "
            "dictionary\n");

  assert(catbsd_xpc_dictionary_remove(d, "signed") == CATBSD_XPC_SUCCESS);
  assert(catbsd_xpc_dictionary_count(d) == 5);
  assert(catbsd_xpc_dictionary_remove(d, "signed") == CATBSD_XPC_EINVAL);
  printf(OK "remove drops the key once and reports the second attempt\n");

  catbsd_xpc_release(d);
  printf("  All dictionary tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void test_serialization(void) {
  printf("Testing XPC wire serialization...\n");

  catbsd_xpc_object_t d = catbsd_xpc_dictionary_create();
  void *buf = NULL;
  size_t len = 0;

  catbsd_xpc_dictionary_set_string(d, "Label", "com.catbsd.jobd");
  catbsd_xpc_dictionary_set_int64(d, "PID", 4242);
  catbsd_xpc_dictionary_set_bool(d, "KeepAlive", 1);
  catbsd_xpc_dictionary_set_double(d, "Throttle", -0.0);
  catbsd_xpc_dictionary_set_data(d, "Cookie", "\xDE\xAD\xBE\xEF", 4);

  assert(catbsd_xpc_serialize(d, &buf, &len) == CATBSD_XPC_SUCCESS);
  assert(buf != NULL && len > 24);

  catbsd_xpc_object_t back = catbsd_xpc_deserialize(buf, len);
  assert(back != NULL);
  assert(catbsd_xpc_dictionary_count(back) == 5);
  assert(strcmp(catbsd_xpc_dictionary_get_string(back, "Label"),
                "com.catbsd.jobd") == 0);

  int64_t pid = 0;
  assert(catbsd_xpc_dictionary_get_int64(back, "PID", &pid) ==
             CATBSD_XPC_SUCCESS &&
         pid == 4242);

  /* Bit-exact float round trip: -0.0 must not come back as +0.0, which
   * is what a naive compare-to-zero encoding would produce. */
  double t = 1.0;
  assert(catbsd_xpc_dictionary_get_double(back, "Throttle", &t) ==
         CATBSD_XPC_SUCCESS);
  assert(t == 0.0 && signbit(t));
  printf(OK "%zu-byte frame round-trips all values, -0.0 keeps its sign\n",
         len);

  /* Truncation must be rejected, not half-parsed. */
  assert(catbsd_xpc_deserialize(buf, len - 1) == NULL);
  assert(catbsd_xpc_deserialize(buf, 4) == NULL);
  printf(OK "truncated frames are rejected rather than partially decoded\n");

  /* Corrupt magic must be rejected. */
  ((unsigned char *)buf)[0] = 'X';
  assert(catbsd_xpc_deserialize(buf, len) == NULL);
  printf(OK "a bad magic number is rejected\n");

  free(buf);
  catbsd_xpc_release(back);
  catbsd_xpc_release(d);
  printf("  All serialization tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

/* Echo server: replies to every request with the same payload plus a
 * server-side annotation, so the client can prove the reply is really the
 * answer to its own request. */
static void *echo_server(void *arg) {
  catbsd_xpc_connection_t listener = arg;
  int err = 0;
  catbsd_xpc_connection_t peer =
      catbsd_xpc_connection_accept(listener, 3000, &err);

  if (peer == NULL) {
    return NULL;
  }

  for (;;) {
    catbsd_xpc_object_t req = NULL;
    if (catbsd_xpc_connection_receive_message(peer, &req, 3000) !=
        CATBSD_XPC_SUCCESS) {
      break;
    }

    const char *op = catbsd_xpc_dictionary_get_string(req, "op");
    if (op != NULL && strcmp(op, "quit") == 0) {
      catbsd_xpc_release(req);
      break;
    }

    catbsd_xpc_object_t reply = catbsd_xpc_dictionary_create();
    int64_t seq = -1;
    catbsd_xpc_dictionary_get_int64(req, "seq", &seq);
    catbsd_xpc_dictionary_set_int64(reply, "seq", seq);
    catbsd_xpc_dictionary_set_string(reply, "status", "ok");
    catbsd_xpc_connection_reply(peer, req, reply);

    catbsd_xpc_release(reply);
    catbsd_xpc_release(req);
  }

  catbsd_xpc_connection_release(peer);
  return NULL;
}

static void test_request_reply(void) {
  printf("Testing XPC request/reply over a socket...\n");

  int err = 0;
  catbsd_xpc_connection_t listener =
      catbsd_xpc_connection_create_listener(g_service, &err);
  assert(listener != NULL);
  printf(OK "listener published as service \"%s\"\n", g_service);

  /* A second listener on a live name must be refused, not silently
   * hijack the socket out from under the first. */
  int err2 = 0;
  catbsd_xpc_connection_t dup =
      catbsd_xpc_connection_create_listener(g_service, &err2);
  assert(dup == NULL && err2 == CATBSD_XPC_EEXIST);
  printf(OK "a duplicate listener is refused with %s\n",
         catbsd_xpc_strerror(err2));

  pthread_t server;
  assert(pthread_create(&server, NULL, echo_server, listener) == 0);

  catbsd_xpc_connection_t client =
      catbsd_xpc_connection_create(g_service, &err);
  assert(client != NULL);

  const int rounds = 20;
  for (int n = 0; n < rounds; n++) {
    catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
    catbsd_xpc_object_t reply = NULL;

    catbsd_xpc_dictionary_set_string(req, "op", "echo");
    catbsd_xpc_dictionary_set_int64(req, "seq", n);

    int rc = catbsd_xpc_connection_send_message_with_reply_sync(client, req,
                                                                3000, &reply);
    assert(rc == CATBSD_XPC_SUCCESS);
    assert(reply != NULL);

    /* The reply must carry the request's correlation id and its payload,
     * which is what proves replies aren't just "the next message". */
    assert(catbsd_xpc_message_is_reply(reply));
    assert(catbsd_xpc_message_get_id(reply) ==
           catbsd_xpc_message_get_id(req));

    int64_t seq = -1;
    assert(catbsd_xpc_dictionary_get_int64(reply, "seq", &seq) ==
           CATBSD_XPC_SUCCESS);
    assert(seq == n);

    catbsd_xpc_release(reply);
    catbsd_xpc_release(req);
  }
  printf(OK "%d synchronous request/reply round trips, each reply matched "
            "to its request by correlation id\n",
         rounds);

  catbsd_xpc_object_t quit = catbsd_xpc_dictionary_create();
  catbsd_xpc_dictionary_set_string(quit, "op", "quit");
  catbsd_xpc_connection_send_message(client, quit);
  catbsd_xpc_release(quit);

  pthread_join(server, NULL);
  catbsd_xpc_connection_release(client);
  catbsd_xpc_connection_release(listener);

  /* The listener unlinks its socket on release, so the name is free. */
  catbsd_xpc_connection_t gone = catbsd_xpc_connection_create(g_service, &err);
  assert(gone == NULL && err == CATBSD_XPC_ENOENT);
  printf(OK "releasing the listener unpublishes the service (%s)\n",
         catbsd_xpc_strerror(err));

  printf("  All request/reply tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int received;
  int saw_invalidation;
  int64_t last_seq;
} handler_state_t;

static void event_handler(catbsd_xpc_connection_t conn,
                          catbsd_xpc_object_t msg, void *ctx) {
  handler_state_t *st = ctx;
  (void)conn;

  pthread_mutex_lock(&st->lock);
  if (msg == NULL) {
    st->saw_invalidation = 1;
  } else {
    catbsd_xpc_dictionary_get_int64(msg, "seq", &st->last_seq);
    st->received++;
  }
  pthread_cond_broadcast(&st->cond);
  pthread_mutex_unlock(&st->lock);

  catbsd_xpc_release(msg);
}

static void test_async_event_handler(void) {
  printf("Testing XPC asynchronous event handler...\n");

  char name[80];
  snprintf(name, sizeof(name), "%s.async", g_service);

  int err = 0;
  catbsd_xpc_connection_t listener =
      catbsd_xpc_connection_create_listener(name, &err);
  assert(listener != NULL);

  catbsd_xpc_connection_t client = catbsd_xpc_connection_create(name, &err);
  assert(client != NULL);

  catbsd_xpc_connection_t peer =
      catbsd_xpc_connection_accept(listener, 2000, &err);
  assert(peer != NULL);

  handler_state_t st;
  memset(&st, 0, sizeof(st));
  st.last_seq = -1;
  pthread_mutex_init(&st.lock, NULL);
  pthread_cond_init(&st.cond, NULL);

  assert(catbsd_xpc_connection_set_event_handler(peer, event_handler, &st) ==
         CATBSD_XPC_SUCCESS);
  assert(catbsd_xpc_connection_resume(peer) == CATBSD_XPC_SUCCESS);

  /* Once resumed, the reader thread owns the socket -- a synchronous
   * receive on the same connection must be refused rather than racing it. */
  catbsd_xpc_object_t stray = NULL;
  assert(catbsd_xpc_connection_receive_message(peer, &stray, 0) ==
         CATBSD_XPC_EINVAL);
  printf(OK "synchronous receive is refused on a resumed connection\n");

  const int count = 25;
  for (int n = 0; n < count; n++) {
    catbsd_xpc_object_t msg = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_int64(msg, "seq", n);
    assert(catbsd_xpc_connection_send_message(client, msg) ==
           CATBSD_XPC_SUCCESS);
    catbsd_xpc_release(msg);
  }

  pthread_mutex_lock(&st.lock);
  while (st.received < count) {
    pthread_cond_wait(&st.cond, &st.lock);
  }
  pthread_mutex_unlock(&st.lock);
  assert(st.last_seq == count - 1);
  printf(OK "handler received all %d messages asynchronously, in order "
            "(last seq %lld)\n",
         count, (long long)st.last_seq);

  /* Dropping the client must surface as an invalidation callback, which
   * is how a supervisor learns a daemon died. */
  catbsd_xpc_connection_release(client);

  pthread_mutex_lock(&st.lock);
  while (!st.saw_invalidation) {
    pthread_cond_wait(&st.cond, &st.lock);
  }
  pthread_mutex_unlock(&st.lock);
  printf(OK "peer hangup delivered a NULL-message invalidation (%s)\n",
         catbsd_xpc_strerror(catbsd_xpc_connection_last_error(peer)));

  catbsd_xpc_connection_release(peer);
  catbsd_xpc_connection_release(listener);
  pthread_mutex_destroy(&st.lock);
  pthread_cond_destroy(&st.cond);

  printf("  All async handler tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void test_error_paths(void) {
  printf("Testing XPC error handling...\n");

  int err = 0;

  assert(catbsd_xpc_connection_create("no.such.service.here", &err) == NULL);
  assert(err == CATBSD_XPC_ENOENT);
  printf(OK "connecting to an unpublished service fails cleanly (%s)\n",
         catbsd_xpc_strerror(err));

  /* A service name is one path component: anything that could escape the
   * runtime directory is rejected before it reaches the filesystem. */
  assert(catbsd_xpc_connection_create_listener("../escape", &err) == NULL);
  assert(err == CATBSD_XPC_EINVAL);
  printf(OK "a traversal-shaped service name is rejected (%s)\n",
         catbsd_xpc_strerror(err));

  /* Receive timeout on an idle but healthy connection. */
  char name[80];
  snprintf(name, sizeof(name), "%s.idle", g_service);
  catbsd_xpc_connection_t listener =
      catbsd_xpc_connection_create_listener(name, &err);
  assert(listener != NULL);
  catbsd_xpc_connection_t client = catbsd_xpc_connection_create(name, &err);
  assert(client != NULL);
  catbsd_xpc_connection_t peer =
      catbsd_xpc_connection_accept(listener, 2000, &err);
  assert(peer != NULL);

  catbsd_xpc_object_t msg = NULL;
  uint64_t start = mono_ns();
  int rc = catbsd_xpc_connection_receive_message(peer, &msg, 60);
  uint64_t elapsed_ms = (mono_ns() - start) / 1000000ULL;
  assert(rc == CATBSD_XPC_ETIMEOUT);
  assert(elapsed_ms >= 40 && elapsed_ms < 2000);
  printf(OK "idle receive timed out after ~%llums (requested 60ms)\n",
         (unsigned long long)elapsed_ms);

  /* accept() on a listener nobody connects to must also time out. */
  start = mono_ns();
  catbsd_xpc_connection_t nobody =
      catbsd_xpc_connection_accept(listener, 60, &err);
  elapsed_ms = (mono_ns() - start) / 1000000ULL;
  assert(nobody == NULL && err == CATBSD_XPC_ETIMEOUT);
  assert(elapsed_ms >= 40);
  printf(OK "accept timed out after ~%llums with no client\n",
         (unsigned long long)elapsed_ms);

  catbsd_xpc_connection_release(client);
  catbsd_xpc_connection_release(peer);
  catbsd_xpc_connection_release(listener);

  printf("  All error handling tests passed!\n\n");
}

/* ------------------------------------------------------------------ */

static void demo_throughput(void) {
  printf("Measuring XPC throughput...\n");

  char name[80];
  snprintf(name, sizeof(name), "%s.bench", g_service);

  int err = 0;
  catbsd_xpc_connection_t listener =
      catbsd_xpc_connection_create_listener(name, &err);
  catbsd_xpc_connection_t client = catbsd_xpc_connection_create(name, &err);
  catbsd_xpc_connection_t peer =
      catbsd_xpc_connection_accept(listener, 2000, &err);
  assert(listener && client && peer);

  const int total = 2000;
  uint64_t start = mono_ns();

  for (int n = 0; n < total; n++) {
    catbsd_xpc_object_t msg = catbsd_xpc_dictionary_create();
    catbsd_xpc_dictionary_set_int64(msg, "seq", n);
    catbsd_xpc_dictionary_set_string(msg, "payload", "cat facts go here");
    catbsd_xpc_connection_send_message(client, msg);
    catbsd_xpc_release(msg);

    catbsd_xpc_object_t got = NULL;
    assert(catbsd_xpc_connection_receive_message(peer, &got, 2000) ==
           CATBSD_XPC_SUCCESS);
    catbsd_xpc_release(got);
  }

  uint64_t elapsed = mono_ns() - start;
  printf("  Sent and received %d messages in %llu microseconds\n", total,
         (unsigned long long)(elapsed / 1000));
  printf("  Average: %llu ns per message\n",
         (unsigned long long)(elapsed / (uint64_t)total));

  catbsd_xpc_connection_release(client);
  catbsd_xpc_connection_release(peer);
  catbsd_xpc_connection_release(listener);
  printf("\n");
}

int main(void) {
  printf("=== CatBSD XPC Shim Test ===\n\n");

  make_service_name();

  test_dictionary_types();
  test_serialization();
  test_request_reply();
  test_async_event_handler();
  test_error_paths();
  demo_throughput();

  printf("=== All tests passed! ===\n");
  printf("\nDarwin XPC service semantics -- named rendezvous, typed\n");
  printf("messages, correlated replies -- over plain Unix sockets.\n");
  return 0;
}
