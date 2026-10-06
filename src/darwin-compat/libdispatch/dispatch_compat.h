/*
 * CatBSD Grand Central Dispatch Compatibility Header
 *
 * Maps standard Darwin dispatch_* API names to catbsd_dispatch_*
 * implementation symbols on non-Apple platforms.
 */

#ifndef _CATBSD_DISPATCH_COMPAT_H_
#define _CATBSD_DISPATCH_COMPAT_H_

#ifdef __APPLE__
#error "dispatch_compat.h is for non-Apple platforms only; use <dispatch/dispatch.h> on macOS"
#endif

#include "dispatch_shim.h"

#define dispatch_queue_t               catbsd_dispatch_queue_t
#define dispatch_queue_attr_t          catbsd_dispatch_queue_attr_t
#define DISPATCH_QUEUE_SERIAL          CATBSD_DISPATCH_QUEUE_SERIAL
#define DISPATCH_QUEUE_CONCURRENT      CATBSD_DISPATCH_QUEUE_CONCURRENT
#define DISPATCH_QUEUE_PRIORITY_HIGH   CATBSD_DISPATCH_QUEUE_PRIORITY_HIGH
#define DISPATCH_QUEUE_PRIORITY_DEFAULT CATBSD_DISPATCH_QUEUE_PRIORITY_DEFAULT
#define DISPATCH_QUEUE_PRIORITY_LOW    CATBSD_DISPATCH_QUEUE_PRIORITY_LOW
#define DISPATCH_QUEUE_PRIORITY_BACKGROUND CATBSD_DISPATCH_QUEUE_PRIORITY_BACKGROUND
#define DISPATCH_TIME_NOW              CATBSD_DISPATCH_TIME_NOW
#define DISPATCH_TIME_FOREVER          CATBSD_DISPATCH_TIME_FOREVER

#define dispatch_queue_create          catbsd_dispatch_queue_create
#define dispatch_get_global_queue      catbsd_dispatch_get_global_queue
#define dispatch_get_main_queue        catbsd_dispatch_get_main_queue
#define dispatch_queue_get_label       catbsd_dispatch_queue_get_label
#define dispatch_retain                catbsd_dispatch_retain
#define dispatch_release               catbsd_dispatch_release

#define dispatch_async                 catbsd_dispatch_async
#define dispatch_async_f               catbsd_dispatch_async_f
#define dispatch_sync                  catbsd_dispatch_sync
#define dispatch_sync_f                catbsd_dispatch_sync_f
#define dispatch_barrier_async         catbsd_dispatch_barrier_async
#define dispatch_barrier_async_f       catbsd_dispatch_barrier_async_f
#define dispatch_barrier_sync          catbsd_dispatch_barrier_sync
#define dispatch_barrier_sync_f        catbsd_dispatch_barrier_sync_f
#define dispatch_after                 catbsd_dispatch_after
#define dispatch_after_f               catbsd_dispatch_after_f
#define dispatch_apply                 catbsd_dispatch_apply
#define dispatch_apply_f               catbsd_dispatch_apply_f

#define dispatch_group_t               catbsd_dispatch_group_t
#define dispatch_group_create          catbsd_dispatch_group_create
#define dispatch_group_enter           catbsd_dispatch_group_enter
#define dispatch_group_leave           catbsd_dispatch_group_leave
#define dispatch_group_wait            catbsd_dispatch_group_wait
#define dispatch_group_async           catbsd_dispatch_group_async
#define dispatch_group_async_f         catbsd_dispatch_group_async_f

#define dispatch_semaphore_t           catbsd_dispatch_semaphore_t
#define dispatch_semaphore_create      catbsd_dispatch_semaphore_create
#define dispatch_semaphore_signal      catbsd_dispatch_semaphore_signal
#define dispatch_semaphore_wait        catbsd_dispatch_semaphore_wait

#define dispatch_once_t                catbsd_dispatch_once_t
#define dispatch_once                  catbsd_dispatch_once
#define dispatch_once_f                catbsd_dispatch_once_f

#define dispatch_source_t              catbsd_dispatch_source_t
#define dispatch_source_type_t         catbsd_dispatch_source_type_t
#define DISPATCH_SOURCE_TYPE_TIMER     CATBSD_DISPATCH_SOURCE_TYPE_TIMER
#define dispatch_source_create         catbsd_dispatch_source_create
#define dispatch_source_set_timer      catbsd_dispatch_source_set_timer
#define dispatch_source_set_event_handler catbsd_dispatch_source_set_event_handler
#define dispatch_source_set_event_handler_f catbsd_dispatch_source_set_event_handler_f
#define dispatch_source_set_cancel_handler catbsd_dispatch_source_set_cancel_handler
#define dispatch_source_set_cancel_handler_f catbsd_dispatch_source_set_cancel_handler_f
#define dispatch_source_cancel         catbsd_dispatch_source_cancel
#define dispatch_resume                catbsd_dispatch_resume
#define dispatch_suspend               catbsd_dispatch_suspend
#define dispatch_time                  catbsd_dispatch_time

#endif /* _CATBSD_DISPATCH_COMPAT_H_ */
