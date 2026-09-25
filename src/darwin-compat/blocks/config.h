/*
 * config.h — hand-written for CatBSD (replaces autoconf output)
 *
 * We target clang/gcc on macOS, FreeBSD, and Linux.  All three support
 * __sync_bool_compare_and_swap, so we use that atomic path.
 *
 * On Apple platforms the compiler already ships a Blocks runtime inside
 * libSystem; building this file there is only useful for testing.  We
 * deliberately avoid the OSAtomic path (HAVE_OSATOMIC_COMPARE_AND_SWAP_*)
 * because OSAtomic.h is deprecated since macOS 10.12 and absent on FreeBSD.
 */

#ifndef _CATBSD_BLOCKS_CONFIG_H_
#define _CATBSD_BLOCKS_CONFIG_H_

/* Use GCC/clang built-in compare-and-swap — available on every target. */
#define HAVE_SYNC_BOOL_COMPARE_AND_SWAP_INT  1
#define HAVE_SYNC_BOOL_COMPARE_AND_SWAP_LONG 1

/* No Objective-C GC support needed. */
/* #undef HAVE_OBJC_WEAK */

/* No AvailabilityMacros / TargetConditionals needed outside Apple SDK. */
#ifdef __APPLE__
#  define HAVE_AVAILABILITY_MACROS_H 1
#  define HAVE_TARGET_CONDITIONALS_H 1
#endif

#endif /* _CATBSD_BLOCKS_CONFIG_H_ */
