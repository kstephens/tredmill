#ifndef _tredmill_INTERNAL_H
#define _tredmill_INTERNAL_H

/* $Id: internal.h,v 1.16 2008-01-16 04:14:07 stephens Exp $ */

/****************************************************************************/

#include "tm.h"
#include <limits.h> /* PAGESIZE */
#include <sys/time.h> /* struct timeval */
#include <setjmp.h>
#include "list.h"

#include "tredmill/debug.h"
#include "tredmill/stats.h"
#include "tredmill/barrier.h"

#include "util/bitset.h" /* bitset_t */

/****************************************************************************/
/* Configuration */

#ifndef PAGESIZE
#define PAGESIZE (1 << 13) /* 8192 */
#endif

#ifndef tm_PAGESIZE
#define tm_PAGESIZE PAGESIZE
#endif

#ifndef tm_TIME_STAT
#define tm_TIME_STAT 1 /* Enable timing stats. */
#endif

#ifndef tm_name_GUARD
#define tm_name_GUARD 0
#endif

#ifndef tm_block_GUARD
#define tm_block_GUARD 0
#endif

/****************************************************************************/
/* Color */


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
#define tm_node_ptr(n) ((void*)(((tm_node*) n) + 1))


/****************************************************************************/
/*
** A tm_block is always aligned to tm_block_SIZE.
*/
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
**
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


#define tm_block_SIZE_MASK ~(((unsigned long) tm_block_SIZE) - 1)
#define tm_page_SIZE_MASK ~(((unsigned long) tm_page_SIZE) - 1)

/****************************************************************************/
/* Node Iterator. */

typedef struct tm_node_iterator {
  int color;
  tm_node *node_next;
  tm_type *type;
  tm_node *node;
  void *ptr;    /* Used for scanning node interiors. */
  void *end;    /* End of scannable region. */
  size_t size;  /* Size of scannable region. */
} tm_node_iterator;


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


/* Internal data. */
struct tm_data {
  /* Current status. */
  int inited, initing;

  /* The current processing phase. */
  enum tm_phase phase, next_phase;

  /* Possible actions during current phase. */
  int marking;
  int scanning;
  int sweeping;
  int unmarking;

  /* Global node counts. */
  size_t n[tm__LAST3];

  /* Number of cumulative allocations during each phase. */
  size_t alloc_by_phase[tm_phase_END];

  /* Current tm_alloc() list change stats. */
  size_t alloc_n[tm__LAST3];
  
  /* Types. */
  tm_type types; /* List of all types. */
  int type_id;

  /* Type hash table. */
  tm_type type_reserve[50], *type_free;
#ifndef tm_type_hash_LEN
#define tm_type_hash_LEN 101
#endif
  tm_type *type_hash[tm_type_hash_LEN];

  /* Block. */
  tm_block *block_first; /* The first block allocated. */
  tm_block *block_last;  /* The last block allocated. */

  /* A list of free tm_blocks not returned to os. */
  tm_list free_blocks;
  int free_blocks_n;

  /* Block sweeping iterators. */
  tm_type *bt;
  tm_block *bb;

  /* OS-level allocation */
  void * os_alloc_last;   /* The last alloc from the os. */
  size_t os_alloc_last_size; 
  void * os_alloc_expected; /* The next ptr expected from _tm_os_alloc_(). */

#define tm_BITMAP_SIZE (tm_address_range_k / (tm_page_SIZE / 1024) / bitset_ELEM_BSIZE)

  /* Valid pointer range. */
  void *ptr_range[2];

  /* A bit map of pages with nodes in use. */
  /* Addressable by tm_page_SIZE. */
  bitset_t page_in_use[tm_BITMAP_SIZE];

  /* A bit map of pages containing allocated block headers. */
  bitset_t block_header[tm_BITMAP_SIZE];

  /* A bit map of large blocks. */
  bitset_t block_large[tm_BITMAP_SIZE];

  /* Type color list iterators. */
  tm_node_iterator node_color_iter[tm_TOTAL];

  /* Partial scan buffer. */
  tm_node *scan_node;
  void *scan_ptr;
  size_t *scan_size;

  /* Roots */
  tm_root roots[16];
  tm_root aroots[16]; /* anti-root */
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
/* Global node color list iterators. */


static __inline
void tm_node_iterator_init(tm_node_iterator *ni)
{
  ni->type = (void *) &tm.types;
  ni->node_next = (void *) &ni->type->color_list[ni->color];
  ni->node = 0;
  ni->ptr = 0;
  ni->end = 0;
  ni->size = 0;
}

static __inline
tm_node * tm_node_iterator_next(tm_node_iterator *ni)
{
  size_t i = 0;

  while ( i ++ < tm.n[ni->color] ) {
    // fprintf(stderr, "  t%p n%p i%ul\n", ni->type, ni->node_next, i);
  
    /* Wrap around on types? */
    if ( (void *) ni->type == (void *) &tm.types ) {
      ni->type = (void*) tm_list_next(ni->type);

      /* This should never happen */
      if ( (void *) ni->type == (void *) &tm.types ) {
	tm_abort();
	return 0;
      }
      
    next_type:
      /* Start on type node list. */
      ni->node_next = (void *) &ni->type->color_list[ni->color];
      
      /* Move iterator to next node. */
      ni->node_next = (void *) tm_list_next(ni->node_next);
    }

    /* At end of type color list? */
    if ( (void *) ni->node_next == (void *) &ni->type->color_list[ni->color] ) {
      ni->type = (void*) tm_list_next(ni->type);
      goto next_type;
    }

    if ( tm_node_color(ni->node_next) != ni->color ) {
      fprintf(stderr, "  node_color_iter[%d] derailed\n", ni->color);
      ni->type = (void*) tm_list_next(ni->type);
      goto next_type;
    }
    else {
      /* Return current node. */
      ni->node = ni->node_next;
      
      /* Move iterator to next node. */
      ni->node_next = (void*) tm_list_next(ni->node_next);
      
      return ni->node;
    }
  }

  return 0;
}

#define tm_node_LOOP_INIT(C) \
  tm_node_iterator_init(&tm.node_color_iter[C])

#define tm_node_LOOP(C) {						\
  tm_node *n;								\
  tm_type *t = 0;							\
  while ( (n = tm_node_iterator_next(&tm.node_color_iter[C])) ) {	\
    t = tm.node_color_iter[C].type;					\
    {

#define tm_node_LOOP_BREAK(C) break

#define tm_node_LOOP_END(C)			\
    }						\
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
