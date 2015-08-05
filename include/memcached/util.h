#ifndef UTIL_H
#define UTIL_H
/*
 * Wrappers around strtoull/strtoll that are safer and easier to
 * use.  For tests and assumptions, see internal_tests.c.
 *
 * str   a NULL-terminated base decimal 10 unsigned integer
 * out   out parameter, if conversion succeeded
 *
 * returns true if conversion succeeded.
 */
#include <memcached/visibility.h>
#include <memcached/protocol_binary.h>
#ifdef __cplusplus
extern "C" {
#endif

MEMCACHED_PUBLIC_API bool safe_strtoull(const char *str, uint64_t *out);
MEMCACHED_PUBLIC_API bool safe_strtoll(const char *str, int64_t *out);
MEMCACHED_PUBLIC_API bool safe_strtoul(const char *str, uint32_t *out);
MEMCACHED_PUBLIC_API bool safe_strtol(const char *str, int32_t *out);
MEMCACHED_PUBLIC_API bool safe_strtof(const char *str, float *out);

MEMCACHED_PUBLIC_API
const char *memcached_protocol_errcode_2_text(protocol_binary_response_status err);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
