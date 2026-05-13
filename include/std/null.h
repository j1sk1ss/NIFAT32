/*
License:
    MIT License. See LICENSE file in project root.
    Copyright (c) 2025 Nikolay

Description:
    Portable NULL definition for C and C++ consumers.

Dependencies:
    - None.
*/

#ifndef NULL_H_
#define NULL_H_

#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED1(x) (void)(x)
#define UNUSED2(x, y) (void)(x), (void)(y)
#define UNUSED3(x, y, z) (void)(x), (void)(y), (void)(z)
#define UNUSED4(a, x, y, z) (void)(a), (void)(x), (void)(y), (void)(z)
#define UNUSED5(a, b, x, y, z) (void)(a), (void)(b), (void)(x), (void)(y), (void)(z)

#define VA_NUM_ARGS_IMPL(_1, _2, _3, _4, _5, N, ...) N
#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__, 5, 4, 3, 2, 1)

#define UNUSED_IMPL_(nargs) UNUSED##nargs
#define UNUSED_IMPL(nargs) UNUSED_IMPL_(nargs)
#define UNUSED(...) UNUSED_IMPL(VA_NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

#ifdef __cplusplus
  #if __cplusplus >= 201103L
    #define NULL nullptr
  #else
    #define NULL 0
  #endif
#else
#ifndef NULL
  #define NULL ((void*)0)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
