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
  #define NULL ((void*)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
