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
