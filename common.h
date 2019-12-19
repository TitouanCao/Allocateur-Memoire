#ifndef __COMMON_H__
#define __COMMON_H__
#include <stdlib.h>

#ifdef DEBUG
#  include <stdio.h>
#  define debug(...) ((void)fprintf(stderr, __VA_ARGS__))
#else
#  define debug(...) ((void)0)
#endif

void *get_memory_adr();
size_t get_memory_size();
void *alloc_max(size_t estimate);

#endif
