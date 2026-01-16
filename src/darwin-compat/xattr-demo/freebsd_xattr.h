/*
 * FreeBSD xattr compatibility wrapper
 * Translates macOS xattr API to FreeBSD extattr API
 */

#ifdef __FreeBSD__

#include <errno.h>
#include <string.h>
#include <sys/extattr.h>
#include <sys/types.h>

/* Wrapper: macOS listxattr -> FreeBSD extattr_list_file */
static inline ssize_t listxattr(const char *path, char *namebuf, size_t size,
                                int options) {
  (void)options; /* FreeBSD doesn't use options */
  return extattr_list_file(path, EXTATTR_NAMESPACE_USER, namebuf, size);
}

/* Wrapper: macOS getxattr -> FreeBSD extattr_get_file */
static inline ssize_t getxattr(const char *path, const char *name, void *value,
                               size_t size, u_int32_t position, int options) {
  (void)position; /* FreeBSD doesn't support position */
  (void)options;
  return extattr_get_file(path, EXTATTR_NAMESPACE_USER, name, value, size);
}

/* Wrapper: macOS setxattr -> FreeBSD extattr_set_file */
static inline int setxattr(const char *path, const char *name,
                           const void *value, size_t size, u_int32_t position,
                           int options) {
  (void)position;
  (void)options;
  return extattr_set_file(path, EXTATTR_NAMESPACE_USER, name, value, size) >= 0
             ? 0
             : -1;
}

/* Wrapper: macOS removexattr -> FreeBSD extattr_delete_file */
static inline int removexattr(const char *path, const char *name, int options) {
  (void)options;
  return extattr_delete_file(path, EXTATTR_NAMESPACE_USER, name);
}

#endif /* __FreeBSD__ */
