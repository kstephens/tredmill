/* $Id: tm.h,v 1.7 2000-01-13 11:19:01 stephensk Exp $ */
#ifndef _tredmill_tm_h
#define _tredmill_tm_h

/*******************************************************************************/
/* Headers */

#include <stddef.h> /* size_t */
#include <stdio.h> /* FILE */

/*******************************************************************************/
/* Allocation descriptor */

typedef struct tm_adesc {
  size_t size;
  void *opaque; /* user-specified data. */
  void *hidden; /* tm_type handle. */
} tm_adesc;

tm_adesc *tm_adesc_for_size(tm_adesc *desc, int force_new);

/*******************************************************************************/
/* Initialization. */

void tm_init(int *argcp, char ***argvp, char ***envpp);

/*******************************************************************************/
/* Roots */

int  tm_root_add(const char *name, const void *begin, const void *end);
void tm_root_remove(const char *name, const void *begin, const void *end);

/*******************************************************************************/
/* Allocation. */

void *tm_alloc(size_t size);
void *tm_alloc_desc(tm_adesc *desc);
void *tm_realloc(void *ptr, size_t size);
void tm_free(void *ptr);

/*******************************************************************************/
/* Current status. */

extern int tm_marking;
extern int tm_sweeping;

/*******************************************************************************/
/* Write barrier */

/*
** Assumes referent is a stack or data segment location.
*/
extern void (*_tm_write_root)(void *referent);
#define tm_write_root(X)(*_tm_write_root)(X)

/*
** Assumes referent is either a pointer to the stack, to a data segment, or within
** a tm_alloc()'ed block.
*/
extern void (*_tm_write_barrier)(void *referent);
#define tm_write_barrier(R) (*_tm_write_barrier)(R)

extern void (*_tm_write_barrier_pure)(void *referent);
#define tm_write_barrier_pure(R) (*_tm_write_barrier_pure)(R)
/* Assumes referent is a tm_alloc() ptr. */

/*******************************************************************************/
/* GC */

void tm_gc_full();

/*******************************************************************************/
/* Debugging */

extern int tm_sweep_is_error;

extern FILE *tm_msg_file;
extern const char *tm_msg_prefix;

void tm_msg_enable(const char *codes, int enable);
void tm_msg(const char *format, ...);
void tm_msg1(const char *format, ...);
void tm_print_stats();
void tm_print_block_stats();
void tm_print_time_stats();


/*******************************************************************************/
/* Control. */

extern long tm_node_alloc_some_size;
extern long tm_root_scan_some_size;
extern long tm_node_scan_some_size;
extern long tm_node_sweep_some_size;
extern long tm_block_sweep_some_size;
extern long tm_node_unmark_some_size;
extern unsigned long tm_alloc_id;

/*******************************************************************************/
/* EOF */

#endif
