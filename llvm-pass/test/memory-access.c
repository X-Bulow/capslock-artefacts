#include <stdatomic.h>
#include <string.h>

struct S {
    int a;
    long b;
};

void memory_access_cases(
    struct S *p,
    char *byte_ptr,
    long *long_ptr,
    _Atomic int *rmw_target,
    _Atomic int *cas_target,
    char *dst,
    const char *src)
{
    *byte_ptr = 1;
    *long_ptr = 2;
    p->b = 3;
    atomic_fetch_add(rmw_target, 1);
    int expected = 0;
    atomic_compare_exchange_strong(cas_target, &expected, 1);
    memcpy(dst, src, 16);
}
