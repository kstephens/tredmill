#ifndef _tredmill_INTERNAL_H
#define _tredmill_INTERNAL_H

/* $Id: internal.h,v 1.5 1999-12-28 23:44:19 stephensk Exp $ */

/****************************************************************************/

#include "tm.h"
#include <sys/time.h> /* struct timeval */
#include <setjmp.h>
#include "list.h"

/****************************************************************************/

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
  tm_B = tm__LAST, /* Total blocks. */
  tm_NU,           /* Total nodes in use. */
  tm_b,            /* Total bytes in use. */
  tm_b_NU,         /* Avg bytes/node. */
  tm__LAST2,

  /* Aliases for internal structure coloring. */
  tm_FREE_BLOCK = tm_WHITE,
  tm_LIVE_BLOCK = tm_ECRU,
  tm_FREE_TYPE  = tm_GREY,
  tm_LIVE_TYPE  = tm_BLACK

} tm_color;

typedef struct tm_node {
  tm_list list;         /* The current list for the node. */
} tm_node;

#ifndef tm_name_GUARD
#define tm_name_GUARD 0
#endif

#ifndef tm_block_GUARD
#define tm_block_GUARD 0
#endif

/*
** A tm_block is always aligned to tm_block_SIZE.
*/
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

enum tm_config {
  tm_ALLOC_ALIGN = 8, /* Nothing smaller than this is actually allocated. */

  tm_PTR_ALIGN = __alignof(void*),

  tm_node_HDR_SIZE = sizeof(tm_node),
  tm_block_HDR_SIZE = sizeof(tm_block),

  tm_block_SIZE = 8 * 1024,

  tm_block_SIZE_MAX = tm_block_SIZE - tm_block_HDR_SIZE,

  tm_PTR_RANGE = 512 * 1024 * 1024, /* 512 meg */
  tm_block_N_MAX = tm_PTR_RANGE / tm_block_SIZE,
};

/****************************************************************************/
/* Internal data. */

enum tm_phase {
  tm_ALLOC = 0, /* Alloc from free.                       (WHITE->ECRU) */
  tm_ROOT,      /* Begin mark roots, alloc os.            (ECRU->GREY)  */
  tm_MARK,      /* Marking marked roots, alloc os.        (GREY->BLACK) */
  tm_SWEEP,     /* Sweepng unmarked nodes, alloc free/os. (ECRU->WHITE) */
  tm_UNMARK,    /* Ummarking marked roots, alloc free/os. (BLACK->ECRU) */
};

struct tm_data {
  /* Valid pointer range. */
  void *ptr_range[2];

  /* The current process. */
  enum tm_phase phase;
  int next_phase;

  /* Block. */
  void *block_base;
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
  size_t n[tm__LAST2];

  /* Type color list iterators. */
  tm_type *tp[tm_TOTAL];
  tm_node *np[tm_TOTAL];

  /* Current tm_alloc() list change stats. */
  size_t alloc_n[tm__LAST2];
  
  /* tm_alloc() timing. */
  struct timeval ts, td;

  /* Roots */
  struct {
    const char *name;
    const void *l, *h;
  } roots[8];
  int nroots;

  /* Current root mark. */
  int rooti;
  const char *rp;

  /* How many root mutations happened since the ROOT phase. */
  unsigned long global_mutations;
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
