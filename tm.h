#ifndef _tredmill_TM_H
#define _tredmill_TM_H

#include <stddef.h> /* size_t */

void tm_init(int *argcp, char ***argvp, char ***envpp);

void *tm_alloc(size_t size);
void *tm_realloc(void *ptr, size_t size);
void tm_free(void *ptr);

extern int tm_marking;
extern int tm_sweeping;

void tm_root_write(void **ptrp);
void tm_mark(void *ptrp);

void _tm_write_barrier(void *referent);
#define tm_write_barrier(R) do{ void*__p = (R); if(tm_marking)_tm_write_barrier(__p); else if ( tm_sweeping ) tm_mark(__p); }while(0)

void _tm_write_barrier_pure(void *referent);
#define tm_write_barrier_pure(R) do{if(tm_marking)_tm_write_barrier_pure(R);}while(0)

void tm_gc_full();

extern int tm_sweep_is_error;
void tm_msg(const char *format, ...);
void tm_msg1(const char *format, ...);
void tm_print_stats();

extern long tm_scan_some_roots_size;
extern long tm_scan_some_nodes_size;

#endif
