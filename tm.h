#ifndef _tredmill_TM_H
#define _tredmill_TM_H

#include <stddef.h> /* size_t */
#include <stdio.h> /* FILE */

/* Allocations descriptors */
typedef struct tm_adesc {
  size_t size;
  void *opaque; /* user-specified data. */
  void *hidden; /* tm_type handle. */
} tm_adesc;

tm_adesc *tm_adesc_for_size(tm_adesc *desc, int force_new);

/* Initialization. */
void tm_init(int *argcp, char ***argvp, char ***envpp);

/* Allocation. */
void *tm_alloc(size_t size);
void *tm_alloc_desc(tm_adesc *desc);
void *tm_realloc(void *ptr, size_t size);
void tm_free(void *ptr);

/* Current status. */

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

extern FILE *tm_msg_file;
extern const char *tm_msg_prefix;

void tm_msg(const char *format, ...);
void tm_msg1(const char *format, ...);
void tm_print_stats();
void tm_print_block_stats();

/* Control. */
extern long tm_node_alloc_some_size;
extern long tm_root_scan_some_size;
extern long tm_node_scan_some_size;
extern long tm_node_sweep_some_size;
extern long tm_block_sweep_some_size;
extern long tm_node_unmark_some_size;

#endif
