/*
 * CatBSD Grand Central Dispatch (GCD) Compatibility Shim
 *
 * Implements a clean-room, pthread-backed subset of Darwin's libdispatch.
 * Provides serial & concurrent queues, synchronous/asynchronous execution,
 * barriers, groups, semaphores, once, apply, and timer dispatch sources.
 */

#ifndef _CATBSD_DISPATCH_SHIM_H_
#define _CATBSD_DISPATCH_SHIM_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Blocks runtime integration if supported by compiler */
#if (defined(__has_feature) && __has_feature(blocks)) || defined(__BLOCKS__)
#include <Block.h>
#define CATBSD_HAS_BLOCKS 1
#else
#define CATBSD_HAS_BLOCKS 0
#endif

/* Queue attributes */
struct catbsd_dispatch_queue_attr_s;
typedef const struct catbsd_dispatch_queue_attr_s *catbsd_dispatch_queue_attr_t;

extern const struct catbsd_dispatch_queue_attr_s _catbsd_dispatch_queue_attr_serial;
extern const struct catbsd_dispatch_queue_attr_s _catbsd_dispatch_queue_attr_concurrent;

#define CATBSD_DISPATCH_QUEUE_SERIAL     (&_catbsd_dispatch_queue_attr_serial)
#define CATBSD_DISPATCH_QUEUE_CONCURRENT (&_catbsd_dispatch_queue_attr_concurrent)

/* Queue priorities for global queues */
#define CATBSD_DISPATCH_QUEUE_PRIORITY_HIGH        2
#define CATBSD_DISPATCH_QUEUE_PRIORITY_DEFAULT     0
#define CATBSD_DISPATCH_QUEUE_PRIORITY_LOW        (-2)
#define CATBSD_DISPATCH_QUEUE_PRIORITY_BACKGROUND (-32768)

/* Time constants */
#define CATBSD_DISPATCH_TIME_NOW     0ULL
#define CATBSD_DISPATCH_TIME_FOREVER (~0ULL)

/* Opaque types */
struct catbsd_dispatch_queue_s;
typedef struct catbsd_dispatch_queue_s *catbsd_dispatch_queue_t;

struct catbsd_dispatch_group_s;
typedef struct catbsd_dispatch_group_s *catbsd_dispatch_group_t;

struct catbsd_dispatch_semaphore_s;
typedef struct catbsd_dispatch_semaphore_s *catbsd_dispatch_semaphore_t;

struct catbsd_dispatch_source_s;
typedef struct catbsd_dispatch_source_s *catbsd_dispatch_source_t;

typedef const struct catbsd_dispatch_source_type_s *catbsd_dispatch_source_type_t;
extern const struct catbsd_dispatch_source_type_s _catbsd_dispatch_source_type_timer;
#define CATBSD_DISPATCH_SOURCE_TYPE_TIMER (&_catbsd_dispatch_source_type_timer)

typedef long catbsd_dispatch_once_t;

/* Queue creation and reference counting */
catbsd_dispatch_queue_t catbsd_dispatch_queue_create(const char *label, catbsd_dispatch_queue_attr_t attr);
catbsd_dispatch_queue_t catbsd_dispatch_get_global_queue(long priority, unsigned long flags);
catbsd_dispatch_queue_t catbsd_dispatch_get_main_queue(void);
const char *catbsd_dispatch_queue_get_label(catbsd_dispatch_queue_t queue);

void catbsd_dispatch_retain(void *object);
void catbsd_dispatch_release(void *object);

/* Work submission (function pointer / context) */
void catbsd_dispatch_async_f(catbsd_dispatch_queue_t queue, void *context, void (*work)(void *));
void catbsd_dispatch_sync_f(catbsd_dispatch_queue_t queue, void *context, void (*work)(void *));
void catbsd_dispatch_barrier_async_f(catbsd_dispatch_queue_t queue, void *context, void (*work)(void *));
void catbsd_dispatch_barrier_sync_f(catbsd_dispatch_queue_t queue, void *context, void (*work)(void *));
void catbsd_dispatch_after_f(uint64_t when, catbsd_dispatch_queue_t queue, void *context, void (*work)(void *));
void catbsd_dispatch_apply_f(size_t iterations, catbsd_dispatch_queue_t queue, void *context, void (*work)(void *, size_t));

/* Work submission (blocks, when compiler supports Blocks) */
#if CATBSD_HAS_BLOCKS
void catbsd_dispatch_async(catbsd_dispatch_queue_t queue, void (^block)(void));
void catbsd_dispatch_sync(catbsd_dispatch_queue_t queue, void (^block)(void));
void catbsd_dispatch_barrier_async(catbsd_dispatch_queue_t queue, void (^block)(void));
void catbsd_dispatch_barrier_sync(catbsd_dispatch_queue_t queue, void (^block)(void));
void catbsd_dispatch_after(uint64_t when, catbsd_dispatch_queue_t queue, void (^block)(void));
void catbsd_dispatch_apply(size_t iterations, catbsd_dispatch_queue_t queue, void (^block)(size_t));
#endif

/* Dispatch Groups */
catbsd_dispatch_group_t catbsd_dispatch_group_create(void);
void catbsd_dispatch_group_enter(catbsd_dispatch_group_t group);
void catbsd_dispatch_group_leave(catbsd_dispatch_group_t group);
long catbsd_dispatch_group_wait(catbsd_dispatch_group_t group, uint64_t timeout_ns);
void catbsd_dispatch_group_async_f(catbsd_dispatch_group_t group, catbsd_dispatch_queue_t queue, void *context, void (*work)(void *));
#if CATBSD_HAS_BLOCKS
void catbsd_dispatch_group_async(catbsd_dispatch_group_t group, catbsd_dispatch_queue_t queue, void (^block)(void));
#endif

/* Dispatch Semaphores */
catbsd_dispatch_semaphore_t catbsd_dispatch_semaphore_create(long value);
long catbsd_dispatch_semaphore_signal(catbsd_dispatch_semaphore_t dsema);
long catbsd_dispatch_semaphore_wait(catbsd_dispatch_semaphore_t dsema, uint64_t timeout_ns);

/* Dispatch Once */
void catbsd_dispatch_once_f(catbsd_dispatch_once_t *predicate, void *context, void (*function)(void *));
#if CATBSD_HAS_BLOCKS
void catbsd_dispatch_once(catbsd_dispatch_once_t *predicate, void (^block)(void));
#endif

/* Dispatch Timer Sources */
catbsd_dispatch_source_t catbsd_dispatch_source_create(catbsd_dispatch_source_type_t type,
                                                       uintptr_t handle,
                                                       unsigned long mask,
                                                       catbsd_dispatch_queue_t queue);
catbsd_dispatch_source_t catbsd_dispatch_source_create_timer(catbsd_dispatch_queue_t queue);
void catbsd_dispatch_source_set_timer(catbsd_dispatch_source_t source,
                                      uint64_t start_ns,
                                      uint64_t interval_ns,
                                      uint64_t leeway_ns);
void catbsd_dispatch_source_set_event_handler_f(catbsd_dispatch_source_t source,
                                               void (*handler)(void *context),
                                               void *context);
void catbsd_dispatch_source_set_cancel_handler_f(catbsd_dispatch_source_t source,
                                                void (*handler)(void *context),
                                                void *context);
#if CATBSD_HAS_BLOCKS
void catbsd_dispatch_source_set_event_handler(catbsd_dispatch_source_t source, void (^handler)(void));
void catbsd_dispatch_source_set_cancel_handler(catbsd_dispatch_source_t source, void (^handler)(void));
#endif
void catbsd_dispatch_source_cancel(catbsd_dispatch_source_t source);
void catbsd_dispatch_resume(void *object);
void catbsd_dispatch_suspend(void *object);

/* Time calculation */
uint64_t catbsd_dispatch_time(uint64_t when, int64_t delta_ns);

#ifdef __cplusplus
}
#endif

#endif /* _CATBSD_DISPATCH_SHIM_H_ */
