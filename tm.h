#ifndef _tredmill_TM_H
#define _tredmill_TM_H

#include <stddef.h> /* size_t */

void tm_init(int *argcp, char ***argvp, char ***envpp);

void *tm_alloc(size_t size);
void *tm_realloc(void *ptr, size_t size);
void tm_free(void *ptr);

void tm_write_barrier(void *referent);
void tm_write_barrier_pure(void *referent);
void tm_root_write(void **ptrp);
void tm_gc_full();

extern int tm_sweep_is_error;
void tm_msg(const char *format, ...);
void tm_msg1(const char *format, ...);
void tm_print_stats();

#endif
