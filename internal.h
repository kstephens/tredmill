#ifndef _tredmill_INTERNAL_H
#define _tredmill_INTERNAL_H


/****************************************************************************/

#include <sys/times.h>
#include <setjmp.h>
#include "list.h"

/****************************************************************************/

typedef enum tm_color {
  tm_WHITE,    /* free, in free list. */
  tm_ECRU,     /* allocated, not marked. */
  tm_GREY,     /* marked, not scanned. */
  tm_BLACK,    /* marked and scanned. */
  tm_TOTAL,
  tm__LAST,
  tm_B = tm__LAST, /* number of blocks */
  tm_D,            /* number of definite blocks. */
  tm_PD,           /* % D / B */
  tm_M,            /* number of maybe blocks. */
  tm_PM,           /* % P / B */
  tm_W,            /* waste in bytes. */
  tm_PW,           /* % W / B bytes. */
  tm__LAST2,
} tm_color;

typedef struct tm_node {
  tm_list list;               /* The current list for the node. */
} tm_node;

typedef struct tm_block {
  tm_list list;        /* Type's block list */
  size_t size;          /* Blocks actual size (including this hdr), multiple of tm_block_SIZE. */
  struct tm_type *type; /* The type the block is assigned to. */
  char *alloc;          /* The allocation pointer for this block. */
} tm_block;

typedef struct tm_type {
  tm_list list;               /* All types list. */
  struct tm_type *hash_next;  /* Hash table next ptr. */
  size_t size;                /* Size of each node. */
  tm_list blocks;             /* List of blocks allocated for this type. */
  size_t n[tm__LAST2];         /* Total elements. */
  tm_list l[tm__LAST];        /* Lists of node by color. */
  tm_block *ab;               /* The current block we are allocating from. */
} tm_type;

/****************************************************************************/

enum tm_config {
  tm_ALLOC_ALIGN = 8, /* Nothing smaller than this is actually allocated. */

  tm_PTR_ALIGN = __alignof(void*),

  tm_node_HDR_SIZE = sizeof(tm_node),
  tm_block_HDR_SIZE = sizeof(tm_block),

  tm_block_SIZE = 8 * 1024,

  tm_block_SIZE_MAX = tm_block_SIZE - tm_block_HDR_SIZE,

  tm_PTR_RANGE = 512 * 1024 * 1024,
  tm_block_N_MAX = tm_PTR_RANGE / tm_block_SIZE,
};

/****************************************************************************/
/* Internal data. */

enum tm_phase {
  tm_ALLOC = 0,
  tm_ROOTS,
  tm_SCAN,
  tm_SWEEP,
  tm_UNMARK,
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

  /* Block scanning. */
  tm_type *tb;
  tm_block *bb;

  /* type hash table. */
  tm_type type_reserve[200], *type_free;
#ifndef tm_type_hash_LEN
#define tm_type_hash_LEN 101
#endif
  tm_type *type_hash[tm_type_hash_LEN];

  /* Global counts. */
  size_t n[tm__LAST2];

  /* Type color list iterators. */
  tm_type *tp[tm__LAST];
  tm_node *np[tm__LAST];

  /* Current tm_alloc() list change stats. */
  size_t alloc_n[tm__LAST2];
  
  /* Current tm_alloc() timing. */
  struct tms alloc_time;

  /* Roots */
  struct {
    const char *name;
    const void *l, *h;
  } roots[8];
  int nroots;

  /* Current root scan. */
  int rooti;
  const char *rp;

  /* How many global root mutations happened during SCAN. */
  unsigned long root_mutations;

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
void *tm_realloc_inner(void *ptr, size_t size);
void tm_free_inner(void *ptr);

void tm_gc_full_inner();

/****************************************************************************/
#endif
