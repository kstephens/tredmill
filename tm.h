#ifndef _tredmill_TM_H
#define _tredmill_TM_H

#include <stddef.h> /* size_t */

/* Initialization. */
void tm_init(int *argcp, char ***argvp, char ***envpp);

/* Allocation. */
void *tm_alloc(size_t size);
void *tm_realloc(void *ptr, size_t size);
void tm_free(void *ptr);

extern int tm_marking;
extern int tm_sweeping;

/* Write barrier */

extern void (*_tm_write_root)(void **refp);
#define tm_write_root(X)(*_tm_write_root)(X)

extern void (*_tm_write_barrier)(void *referent);
#define tm_write_barrier(R) (*_tm_write_barrier)(R)

extern void (*_tm_write_barrier_pure)(void *referent);
#define tm_write_barrier_pure(R) (*_tm_write_barrier_pure)(R)

/* GC */
void tm_gc_full();

/* Debugging */
extern int tm_sweep_is_error;
void tm_msg(const char *format, ...);
void tm_msg1(const char *format, ...);
void tm_print_stats();

/* Control. */
extern long tm_scan_some_roots_size;
extern long tm_scan_some_nodes_size;

#endif
