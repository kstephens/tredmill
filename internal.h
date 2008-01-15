#ifndef _tredmill_INTERNAL_H
#define _tredmill_INTERNAL_H

/* $Id: internal.h,v 1.15 2008-01-15 05:21:03 stephens Exp $ */

/****************************************************************************/

#include "tm.h"
#include <sys/time.h> /* struct timeval */
#include <setjmp.h>
#include "list.h"

#include "tredmill/debug.h"
#include "tredmill/barrier.h"

#include "util/bitset.h" /* bitset_t */


#include <limits.h>
#ifndef PAGESIZE
#define PAGESIZE (1 << 13) /* 8192 */
#endif

#ifndef tm_PAGESIZE
#define tm_PAGESIZE PAGESIZE
#endif

/****************************************************************************/
/* Color */


#ifndef tm_TIME_STAT
#define tm_TIME_STAT 1 /* Enable timing stats. */
#endif

typedef enum tm_color {
  /* Node colors */
  tm_WHITE,        /* Free, in free list. */
  tm_ECRU,         /* Allocated. */
  tm_GREY,         /* Scheduled, not marked. */
  tm_BLACK,        /* Marked. */

  /* Stats */
  tm_TOTAL,        /* Total nodes. */
  tm__LAST,

  /* Type-level Stats. */
  tm_B = tm__LAST, /* Total blocks in use. */
  tm_NU,           /* Total nodes in use. */
  tm_b,            /* Total bytes in use. */
  tm_b_NU,         /* Avg bytes/node. */
  tm__LAST2,

  /* Block-level Stats. */
  tm_B_OS = tm__LAST2, /* Blocks currently allocated from OS. */
  tm_b_OS,             /* Bytes currently allocated from OS. */
  tm_B_OS_M,           /* Peak blocks allocated from OS. */
  tm_b_OS_M,           /* Peak bytes allocated from OS. */
  tm__LAST3,

  /* Aliases for internal structure coloring. */
  tm_FREE_BLOCK = tm_WHITE,
  tm_LIVE_BLOCK = tm_ECRU,
  tm_FREE_TYPE  = tm_GREY,
  tm_LIVE_TYPE  = tm_BLACK

} tm_color;

/****************************************************************************/
/* Node */

typedef struct tm_node {
  /* The current list for the node. */
  tm_list list; /* tm_type.color_list[tm_node_color(this)] list */
} tm_node;

#define tm_node_color(n) ((tm_color) tm_list_color(n))


/****************************************************************************/
/*
** A tm_block is always aligned to tm_block_SIZE.
*/

#ifndef tm_name_GUARD
#define tm_name_GUARD 0
#endif

#ifndef tm_block_GUARD
#define tm_block_GUARD 0
#endif

typedef struct tm_block {
  tm_list list;         /* tm_type.block list */

#if tm_block_GUARD
  unsigned long guard1;
#define tm_block_hash(b) (0xe8a624c0 ^ (unsigned long)(b))
#endif

#if tm_name_GUARD
  const char *name;
#endif

  size_t size;          /* Block's actual size (including this hdr), multiple of tm_block_SIZE. */
  struct tm_type *type; /* The type the block is assigned to. */
  char *begin;          /* The beginning of the allocation space. */
  char *end;            /* The end of allocation space. */
  char *alloc;          /* The allocation pointer for new tm_nodes. */
  size_t n[tm__LAST];   /* Total nodes for this block. */

#if tm_block_GUARD
  unsigned long guard2;
#endif

  double alignment;     /* Force alignment to double. */

} tm_block;

#define tm_block_unused(b) ((b)->n[tm_WHITE] == b->n[tm_TOTAL])
#define tm_block_node_begin(b) ((void*) (b)->begin)
#define tm_block_node_end(b) ((void*) (b)->end)
#define tm_block_node_alloc(b) ((void*) (b)->alloc)
#define tm_block_node_size(b) ((b)->type->size + tm_node_HDR_SIZE)
#define tm_block_node_next(b, n) ((void*) (((char*) (n)) + tm_block_node_size(b)))

#if tm_block_GUARD
#define _tm_block_validate(b) do { \
   tm_assert_test(b); \
   tm_assert_test(! ((void*) &tm <= (void*) (b) && (void*) (b) < (void*) (&tm + 1))); \
   tm_assert_test((b)->guard1 == tm_block_hash(b)); \
   tm_assert_test((b)->guard2 == tm_block_hash(b)); \
} while(0)
#else
#define _tm_block_validate(b)
#endif

/****************************************************************************/
/*
** A tm_type represents information about all types of a specific size.
** Keeps track of:
** 1. How many tm_nodes of a given color exists.
** 2. Lists of tm_nodes by color.
** 3. Lists of tm_blocks used.
** 4. The tm_block for initializing new nodes from.
** 5. A tm_adesc user-level descriptor.
*/
typedef struct tm_type {
  tm_list list;               /* All types list: tm.types */
  int id;
#if tm_name_GUARD
  const char *name;
#endif
  struct tm_type *hash_next;  /* Hash table next ptr. */
  size_t size;                /* Size of each node. */
  tm_list blocks;             /* List of blocks allocated for this type. */
  size_t n[tm__LAST2];        /* Totals by color. */
  tm_list color_list[tm_TOTAL];  /* Lists of node by color. */
  tm_block *alloc_from_block; /* The current block we are allocating from. */
  tm_adesc *desc;             /* User-specified descriptor handle. */
} tm_type;

/****************************************************************************/
/* Configuration. */

enum tm_config {
  tm_ALLOC_ALIGN = 8, /* Nothing smaller than this is actually allocated. */

  tm_PTR_ALIGN = __alignof(void*),

  tm_node_HDR_SIZE = sizeof(tm_node),
  tm_block_HDR_SIZE = sizeof(tm_block),

  tm_block_SIZE = tm_PAGESIZE,
  tm_page_SIZE = tm_PAGESIZE,

  tm_block_SIZE_MAX = tm_block_SIZE - tm_block_HDR_SIZE,

  tm_address_range_k = 1UL << (sizeof(void*) * 8 - 10),
  tm_block_N_MAX = sizeof(void*) / tm_block_SIZE,
};


/****************************************************************************/
/* Phases. */

enum tm_phase {
  tm_ALLOC = 0, /* Alloc from free, unmark black (WHITE->ECRU, BLACK->ECRU) */
  tm_ROOT,      /* Begin mark roots, alloc os.            (ECRU->GREY)  */
  tm_SCAN,      /* Scan marked nodes, alloc os.           (GREY->BLACK) */
  tm_SWEEP,     /* Sweepng unmarked nodes, alloc free/os. (ECRU->WHITE) */
  tm_phase_END
};

/****************************************************************************/
/* Root sets. */

typedef struct tm_root {
  const char *name;
  const void *l, *h;
} tm_root;

/****************************************************************************/
/* Stats. */

typedef struct tm_time_stat {
  const char *name;
  double 
    td, /* Last time. */
    ts, /* Total time. */
    tw, /* Worst time. */
    ta, /* Avg time. */
    t0, t1, 
    t01, t11;
  short tw_changed;
  unsigned int count;
} tm_time_stat;

void tm_time_stat_begin(tm_time_stat *ts);
void tm_time_stat_end(tm_time_stat *ts);

/****************************************************************************/
/* Internal data. */

struct tm_data {
  /* Valid pointer range. */
  void *ptr_range[2];

  /****************************************************************************/
  /* Current status. */

  int inited, initing;

  /* Possible actions. */
  int marking;
  int scanning;
  int sweeping;
  int unmarking;

  /* The current processing phase. */
  enum tm_phase phase, next_phase;

  /* Number of cumulative allocations during each phase. */
  size_t alloc_by_phase[tm_phase_END];

  /* Block. */
  tm_block *block_first; /* The first block allocated. */
  tm_block *block_last;  /* The last block allocated. */

  /* OS-level allocation */
  void * os_alloc_last;   /* The last alloc from the os. */
  size_t os_alloc_last_size; 
  void * os_alloc_expected; /* The next ptr expected from _tm_os_alloc_(). */

#define tm_BITMAP_SIZE (tm_address_range_k / (tm_page_SIZE / 1024) / bitset_ELEM_BSIZE)

  /* A bit map of pages with nodes in use. */
  /* Addressable by tm_page_SIZE. */
  bitset_t page_in_use[tm_BITMAP_SIZE];

  /* A bit map of pages containing allocated block headers. */
  bitset_t block_header[tm_BITMAP_SIZE];

  /* A bit map of large blocks. */
  bitset_t block_large[tm_BITMAP_SIZE];

  /* A list of free tm_blocks not returned to os. */
  tm_list free_blocks;
  int free_blocks_n;

  /* Block sweeping iterators. */
  tm_type *bt;
  tm_block *bb;

  /* Types. */
  tm_list types;
  int type_id;

  /* Type hash table. */
  tm_type type_reserve[50], *type_free;
#ifndef tm_type_hash_LEN
#define tm_type_hash_LEN 101
#endif
  tm_type *type_hash[tm_type_hash_LEN];

  /* Global node counts. */
  size_t n[tm__LAST3];

  /* Type color list iterators. */
  tm_type *type_loop_ptr[tm_TOTAL];
  tm_node *node_loop_ptr[tm_TOTAL];

  /* Current tm_alloc() list change stats. */
  size_t alloc_n[tm__LAST3];
  
  /* Stats */
  tm_time_stat /* time spent: */
    ts_os_alloc,            /* in tm_alloc_os(). */
    ts_os_free,             /* in tm_free_os().  */
    ts_alloc,               /* in tm_malloc().    */
    ts_free,                /* in tm_free().     */
    ts_gc,                  /* in tm_gc_full().  */
    ts_barrier,             /* in tm_write_barrier*().    */
    ts_barrier_black,       /* in tm_write_barrier*() recoloring black nodes. */
    ts_phase[tm_phase_END]; /* in each phase.    */
    
  /* Roots */
  tm_root roots[8];
  tm_root aroots[8]; /* anti-root */
  short nroots;
  short naroots;
  short root_datai, root_newi;
  short stack_grows;
  void **stack_ptrp;

  /* Current root mark. */
  short rooti;
  const char *rp;

  /* How many root mutations happened since the ROOT phase. */
  unsigned long data_mutations;
  unsigned long stack_mutations;

  /* Stats/debugging support. */
  size_t alloc_id;
  size_t alloc_pass;
  size_t alloc_request_size;
  tm_type *alloc_request_type;
  int msg_ignored; /* True if last tm_msg() was disabled; used by tm_msg1() */
  char msg_enable_table[256];

  /* Full GC stats. */
  size_t blocks_allocated_since_gc;
  size_t blocks_in_use_after_gc;
  size_t nodes_allocated_since_gc;
  size_t nodes_in_use_after_gc;
  size_t bytes_allocated_since_gc;
  size_t bytes_in_use_after_gc;

  /* mmap() fd. */
  int mmap_fd;

  /* Register roots. */
  jmp_buf jb;
};


extern struct tm_data tm;

#define tm_ptr_l tm.ptr_range[0]
#define tm_ptr_h tm.ptr_range[1]


/**************************************************************************/
/* Global color list iterators. */

#define tm_node_LOOP_INIT(C) do	{		\
  tm.type_loop_ptr[C] = (void *) &tm.types;	\
  tm.node_loop_ptr[C] = 0;			\
} while (0)

#define tm_node_LOOP(C) while ( tm.n[C] ) {				\
    tm_node *n, *node_next;						\
    tm_type *t = tm.type_loop_ptr[C];					\
    /* At end of type loop? */						\
    if ( (void*) t == (void*) &tm.types ) {				\
      /* Advance to next type. */					\
      t = tm.type_loop_ptr[C] = (void*) tm_list_next(t);		\
      tm.node_loop_ptr[C] = tm_list_next(&t->color_list[C]);		\
    }									\
    /* At end of node list? */						\
    if ( (void*) tm.node_loop_ptr[C] == (void*) &t->color_list[C] ) {	\
      /* Advance to next type. */					\
      t = tm.type_loop_ptr[C] = (void*) tm_list_next(t);		\
      tm.node_loop_ptr[C] = tm_list_next(&t->color_list[C]);		\
    } else {								\
      n = tm.node_loop_ptr[C]; node_next = tm_list_next(n);		\
      {

#define tm_node_LOOP_BREAK(C)			\
  /* Advance to next node. */			\
  tm.node_loop_ptr[C] = node_next;		\
  break

#define tm_node_LOOP_END(C)			\
      }						\
    /* Advance to next node. */			\
    tm.node_loop_ptr[C] = node_next;		\
    }						\
}


/****************************************************************************/
/* Internal procs. */


void _tm_phase_init(int p);
void tm_node_set_color(tm_node *n, tm_block *b, tm_color c);

void _tm_set_stack_ptr(void* ptr);

/* Clears the stack and initialize register root set. */
void __tm_clear_some_stack_words();
#define _tm_clear_some_stack_words() \
setjmp(tm.jb); \
__tm_clear_some_stack_words()


void *_tm_alloc_inner(size_t size);
void *_tm_alloc_desc_inner(tm_adesc *desc);
void *_tm_realloc_inner(void *ptr, size_t size);
void _tm_free_inner(void *ptr);

void _tm_gc_full_inner();

/****************************************************************************/
/* Internals */

#include "tredmill/os.h"
#include "tredmill/root.h"
#include "tredmill/page.h"
#include "tredmill/ptr.h"
#include "tredmill/mark.h"

/****************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#endif
