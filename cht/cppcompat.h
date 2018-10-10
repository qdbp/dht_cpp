#ifndef DHT_CPPCOMPAT_H
#define DHT_CPPCOMPAT_H

#ifdef __cplusplus
#define EXTERN_C(syms)                                                         \
    extern "C" {                                                               \
    syms;                                                                      \
    }
#else
#define EXTERN_C(syms) syms;
#endif

#ifdef __cplusplus
#define STATIC_ASSERT(cexpr, msg) static_assert(cexpr, msg);
#else
#define STATIC_ASSERT(cexpr, msg) _Static_assert(cexpr, msg);
#endif

#endif
