#ifndef _tredmill_INTERNAL_H
#define _tredmill_INTERNAL_H

/* $Id: internal.h,v 1.8 2000-01-13 11:19:00 stephensk Exp $ */

/****************************************************************************/

#include "tm.h"
#include <sys/time.h> /* struct timeval */
#include <setjmp.h>
#include "list.h"

#include <limits.h>
#ifndef PAGESIZE
#define PAGESIZE 4096
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
  tm_list list;         /* The current list for the node. */
} tm_node;

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
  tm_list list;         /* Type's block list */

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

  double alignment;     /* Force alignment to double. */

#if tm_block_GUARD
  unsigned long guard2;
#endif

} tm_block;

#define tm_block_node_begin(b) ((void*) (b)->begin)
#define tm_block_node_end(b) ((void*) (b)->end)
#define tm_block_node_alloc(b) ((void*) (b)->alloc)
#define tm_block_node_size(b) ((b)->type->size + tm_node_HDR_SIZE)
#define tm_block_node_next(b, n) ((void*) (((char*) (n)) + tm_block_node_size(b)))

/****************************************************************************/
/*
** A tm_type represents an information about all types of a specific size.
** Keeps track of:
** 1. How many tm_nodes of a given color exists.
** 2. Lists of tm_nodes by color.
** 3. Lists of tm_blocks used.
** 4. The tm_block for initializing new nodes from.
** 5. A tm_adesc user-level descriptor.
*/
typedef struct tm_type {
  tm_list list;               /* All types list. */
#if tm_name_GUARD
  const char *name;
#endif
  struct tm_type *hash_next;  /* Hash table next ptr. */
  size_t size;                /* Size of each node. */
  tm_list blocks;             /* List of blocks allocated for this type. */
  size_t n[tm__LAST2];        /* Total elements. */
  tm_list l[tm_TOTAL];        /* Lists of node by color. */
  tm_block *ab;               /* The current block we are allocating from. */
  tm_adesc *desc;             /* User-specified descriptor handle. */
} tm_type;

/****************************************************************************/
/* Configuration. */

enum tm_config {
  tm_ALLOC_ALIGN = 8, /* Nothing smaller than this is actually allocated. */

  tm_PTR_ALIGN = __alignof(void*),

  tm_node_HDR_SIZE = sizeof(tm_node),
  tm_block_HDR_SIZE = sizeof(tm_block),

  tm_PAGESIZE = PAGESIZE,
  tm_block_SIZE = PAGESIZE,

  tm_block_SIZE_MAX = tm_block_SIZE - tm_block_HDR_SIZE,

  tm_PTR_RANGE = 512 * 1024 * 1024, /* 512 Mb */
  tm_block_N_MAX = tm_PTR_RANGE / tm_block_SIZE,
};

/****************************************************************************/
/* Phases. */

enum tm_phase {
  tm_ALLOC = 0, /* Alloc from free.                       (WHITE->ECRU) */
  tm_ROOT,      /* Begin mark roots, alloc os.            (ECRU->GREY)  */
  tm_MARK,      /* Marking marked roots, alloc os.        (GREY->BLACK) */
  tm_SWEEP,     /* Sweepng unmarked nodes, alloc free/os. (ECRU->WHITE) */
  tm_UNMARK,    /* Ummarking marked roots, alloc free/os. (BLACK->ECRU) */
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
    td, /* Last allocation time. */
    ts, /* Total allocation time. */
    tw, /* Worst allocation time. */
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

  short inited, initing;

  /* The current process. */
  enum tm_phase phase, next_phase;

  /* Block. */
  tm_block *block_first; /* The first block allocated. */
  tm_block *block_last;  /* The last block allocated. */
  void *alloc_os_expected; /* The next ptr expected from tm_alloc_os(). */

#if 0
  unsigned long block_bitmap[tm_block_N_MAX / (sizeof(unsigned long)*8)];
#endif
  tm_list free_blocks;

  /* Types. */
  tm_list types;

  /* Block sweeping iterators. */
  tm_type *bt;
  tm_block *bb;

  /* type hash table. */
  tm_type type_reserve[50], *type_free;
#ifndef tm_type_hash_LEN
#define tm_type_hash_LEN 101
#endif
  tm_type *type_hash[tm_type_hash_LEN];

  /* Global node counts. */
  size_t n[tm__LAST3];

  /* Type color list iterators. */
  tm_type *tp[tm_TOTAL];
  tm_node *np[tm_TOTAL];

  /* Current tm_alloc() list change stats. */
  size_t alloc_n[tm__LAST3];
  
  /* Stats */
  tm_time_stat /* time spent: */
    ts_alloc_os,            /* in tm_alloc_os(). */
    ts_alloc,               /* in tm_alloc().    */
    ts_free,                /* in tm_free().     */
    ts_gc,                  /* in tm_gc_full().  */
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

  /* Register roots. */
  jmp_buf jb;
};


extern struct tm_data tm;

#define tm_ptr_l tm.ptr_range[0]
#define tm_ptr_h tm.ptr_range[1]

/****************************************************************************/
/* Internal procs. */

void tm_set_stack_ptr(void* ptr);

/* Clears the stack and initialize register root set. */
void _tm_clear_some_stack_words();
#define tm_clear_some_stack_words() \
setjmp(tm.jb); \
_tm_clear_some_stack_words()


void *tm_alloc_inner(size_t size);
void *tm_alloc_desc_inner(tm_adesc *desc);
void *tm_realloc_inner(void *ptr, size_t size);
void tm_free_inner(void *ptr);

void tm_gc_full_inner();

/****************************************************************************/
#endif
