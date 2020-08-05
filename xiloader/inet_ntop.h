#ifdef NEED_INET6
#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>

const char * inet_ntop(int af, const void *src, char *dst, size_t size);
#ifdef __cplusplus
}
#endif
#endif
