/** \file internal.h
 * \brief Internals.
 */
#ifndef _tredmill_INTERNAL_H
#define _tredmill_INTERNAL_H

/* $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $ */

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
/*! \defgroup configuration Configuration */
/*@{*/

#ifndef PAGESIZE

/*! The mininum size memory block in the OS. */
#define PAGESIZE (1 << 13) /* 8192 */
#endif

#ifndef tm_PAGESIZE
/*! The size of blocks allocated from the OS. */
#define tm_PAGESIZE PAGESIZE
#endif

#ifndef tm_TIME_STAT
#define tm_TIME_STAT 1 /*!< If true, enable timing stats. */
#endif

#ifndef tm_name_GUARD
#define tm_name_GUARD 0 /*!< If true, enable name guards in internal structures. */
#endif

#ifndef tm_block_GUARD
#define tm_block_GUARD 0 /*!< If true, enable data corruption guards in internal structures. */
#endif


/*@}*/

/****************************************************************************/
/*! \defgroup color Color */
/*@{*/


/**
 * The color of a node.
 *
 * Values greater than tm_BLACK are used as indicies
 * into accounting arrays.
 */
typedef enum tm_color {
  /*! Node colors */

  /*! Free, in free list. */
  tm_WHITE,
  /*! Allocated. */
  tm_ECRU,
  /*! Marked in use, scheduled for interior scanning. */
  tm_GREY,
  /*! Marked in use, scanned. */
  tm_BLACK,        

  /*! Accounting */

  /*! Total nodes of any color. */
  tm_TOTAL,        
  tm__LAST,

  /*! Type-level Stats. */

  /*! Total blocks in use. */
  tm_B = tm__LAST,
  /*! Total nodes in use. */ 
  tm_NU,
  /*! Total bytes in use. */
  tm_b,
  /*! Avg bytes/node. */
  tm_b_NU,
  tm__LAST2,

  /*! Block-level Stats. */

  /*! Blocks currently allocated from OS. */
  tm_B_OS = tm__LAST2,
  /*! Bytes currently allocated from OS. */ 
  tm_b_OS,
  /*! Peak blocks allocated from OS. */
  tm_B_OS_M,
  /*! Peak bytes allocated from OS. */
  tm_b_OS_M,
  tm__LAST3,

  /*! Aliases for internal structure coloring. */

  /*! Color of a free tm_block. */
  tm_FREE_BLOCK = tm_WHITE,
  /*! Color of a live, in-use tm_block. */
  tm_LIVE_BLOCK = tm_ECRU,
  /*! Color of a free tm_type. */
  tm_FREE_TYPE  = tm_GREY,
  /*! Color of a live, in-use tm_type. */
  tm_LIVE_TYPE  = tm_BLACK

} tm_color;


/*@}*/

/****************************************************************************/
/*! \defgroup node Node */
/*@{*/


/**
 * An allocation node.
 *
 * tm_nodes are parceled from tm_blocks.
 * Each tm_node has a color, which places it on
 * a colored list for the tm_node's type.
 */
typedef struct tm_node {
  /*! The current type list for the node. */
  /*! tm_type.color_list[tm_node_color(this)] list. */
  tm_list list; 
} tm_node;

/*! The color of a tm_node. */
#define tm_node_color(n) ((tm_color) tm_list_color(n))
/*! A pointer to the data of a tm_node. */
#define tm_node_ptr(n) ((void*)(((tm_node*) n) + 1))


/*@}*/

/****************************************************************************/
/*! \defgroup block Block */
/*@{*/

/**
 * A block allocated from the operating system.
 * tm_blocks are parceled into tm_nodes of uniform size
 * based on the tm_type.
 *
 * A tm_block is always aligned to tm_block_SIZE.
 */
typedef struct tm_block {
  /*! tm_type.block list */
  tm_list list;

#if tm_block_GUARD
  /*! Magic overwrite guard. */
  unsigned long guard1;
#define tm_block_hash(b) (0xe8a624c0 ^ (unsigned long)(b))
#endif

#if tm_name_GUARD
  /*! Name used for debugging. */
  const char *name;
#endif

  /*! Block's actual size (including this hdr), multiple of tm_block_SIZE. */
  size_t size;         
  /*! The type the block is assigned to.
   *  All tm_nodes parceled from this block will be of type->size.
   */ 
  struct tm_type *type;
  /*! The beginning of the allocation space. */
  char *begin;
  /*! The end of allocation space.  When alloc reaches end, the block is fully parceled. */
  char *end;
  /*! The allocation pointer for new tm_nodes. */
  char *alloc;
  /*! Total nodes for this block. */
  size_t n[tm__LAST];

#if tm_block_GUARD
  /*! Magic overwrite guard. */
  unsigned long guard2;
#endif

  /*! Force alignment of tm_nodes to double. */
  double alignment[0];
} tm_block;

/*! True if the tm_block has no used nodes; i.e. it can be returned to the OS. */
#define tm_block_unused(b) ((b)->n[tm_WHITE] == b->n[tm_TOTAL])
/*! The begining address of any tm_nodes parcelled from a tm_block. */
#define tm_block_node_begin(b) ((void*) (b)->begin)
/*! The end address of any tm_nodes parcelled from a tm_block. */
#define tm_block_node_end(b) ((void*) (b)->end)
/*! The allocation address of the next tm_node to be parcelled from a tm_block. */
#define tm_block_node_alloc(b) ((void*) (b)->alloc)
/*! The total size of a tm_node with a useable size based on the tm_block's tm_type size. */
#define tm_block_node_size(b) ((b)->type->size + tm_node_HDR_SIZE)
/*! The next adddress of a tm_node parcelled from tm_block. */
#define tm_block_node_next(b, n) ((void*) (((char*) (n)) + tm_block_node_size(b)))

#if tm_block_GUARD
/*! Validates a tm_block guards for data corruption. */
#define _tm_block_validate(b) do { \
   tm_assert_test(b); \
   tm_assert_test(! ((void*) &tm <= (void*) (b) && (void*) (b) < (void*) (&tm + 1))); \
   tm_assert_test((b)->guard1 == tm_block_hash(b)); \
   tm_assert_test((b)->guard2 == tm_block_hash(b)); \
} while(0)
#else
#define _tm_block_validate(b)
#endif


/*@}*/

/****************************************************************************/
/*! \defgroup type Type */
/*@{*/

/**
 * A tm_type represents information about all tm_nodes of a specific size.
 *
 * -# How many tm_nodes of a given color exists.
 * -# Lists of tm_nodes by color.
 * -# Lists of tm_blocks used to parcel tm_nodes.
 * -# The tm_block for initializing new nodes from.
 * -# A tm_adesc user-level descriptor.
 * .
 */
typedef struct tm_type {
  /*! All types list: tm.types */
  tm_list list;
  /*! The type id: tm.type_id */
  int id;
#if tm_name_GUARD
  /*! A name for debugging. */
  const char *name;
#endif

  /*! Hash table next ptr: tm.type_hash[]. */
  struct tm_type *hash_next;  
  /*! Size of each tm_node. */
  size_t size;
  /*! List of blocks allocated for this type. */                
  tm_list blocks;     
  /*! Totals by color. */        
  size_t n[tm__LAST2];
  /*! Lists of node by color; see tm_node.list. */
  tm_list color_list[tm_TOTAL];
  /*! The current block we are allocating from. */ 
  tm_block *alloc_from_block;
  /*! User-specified descriptor handle. */ 
  tm_adesc *desc;
} tm_type;


/**
 * Configuration constants.
 */
enum tm_config {
  /*! Nothing smaller than this is actually allocated. */
  tm_ALLOC_ALIGN = 8, 

  /*! Alignment of pointers. */
  tm_PTR_ALIGN = __alignof(void*),

  /*! Size of tm_node headers. */
  tm_node_HDR_SIZE = sizeof(tm_node),
  /*! Size of tm_block headers. */
  tm_block_HDR_SIZE = sizeof(tm_block),

  /*! Size of tm_block. Allocations from operating system are aligned to this size. */
  tm_block_SIZE = tm_PAGESIZE,
  /*! Operating system pages are aligned to this size. */
  tm_page_SIZE = tm_PAGESIZE,

  /*! The maxinum size tm_node that can be allocated from a single tm_block. */
  tm_block_SIZE_MAX = tm_block_SIZE - tm_block_HDR_SIZE,

  /*! The addressable range of process memory in 1024 blocks. */
  tm_address_range_k = 1UL << (sizeof(void*) * 8 - 10),

  /*! Huh? */
  tm_block_N_MAX = sizeof(void*) / tm_block_SIZE,
};


/*! Mask to align tm_block pointers. */ 
#define tm_block_SIZE_MASK ~(((unsigned long) tm_block_SIZE) - 1)
/*! Mask to align page pointers. */ 
#define tm_page_SIZE_MASK ~(((unsigned long) tm_page_SIZE) - 1)

/****************************************************************************/

/*! Colored Node Iterator. */
typedef struct tm_node_iterator {
  /*! The color of nodes to iterate on. */
  int color;
  /*! The next tm_node. */
  tm_node *node_next;
  /*! The current tm_type. */
  tm_type *type;
  /*! The current tm_node. */
  tm_node *node;

  /*! Used for scanning node interiors. */
  /*! The pointer to the tm_node_ptr() */
  void *ptr;
  /*! End of scanable region. */
  void *end;
  /*! Size of scanable region. */
  size_t size;
} tm_node_iterator;


/*@}*/

/****************************************************************************/
/*! \defgroup phase Phase */
/*@{*/

/**
 * The phases of the allocator.
 *
 * These are similar to the phases in 
 * typical in a stop-the-world collector,
 * except that work for these phases is
 * done during allocation by tm_alloc().
 */
enum tm_phase {
  /*! Unmark nodes.           (BLACK->ECRU) */
  tm_UNMARK = 0,
  /*! Begin mark roots.       (ECRU->GREY)  */ 
  tm_ROOT,
  /*! Scan marked nodes.      (GREY->BLACK) */
  tm_SCAN,
  /*! Sweepng unmarked nodes. (ECRU->WHITE) */
  tm_SWEEP,
  /*! Placeholder for size of arrays indexed by tm_phase. */
  tm_phase_END
};


/*@}*/

/****************************************************************************/
/*! \defgroup root_set Root Set */
/*@{*/


/**
 * A root set region to be scanned for possible pointers.
 */
typedef struct tm_root {
  /*! The name of the root. */
  const char *name;
  /*! The low address of the root to be scanned. */
  const void *l;
  /*! The high address of the root to be scanned. */
  const void *h;
} tm_root;


/*@}*/

/****************************************************************************/
/*! \defgroup internal_data Internal: Data */
/*@{*/

/**
 * Internal data for the allocator.
 *
 * This structure is specifically avoided by the uninitialized
 * data segment root, using an anti-root.  See tm_init().
 */
struct tm_data {
  /*! Current status. */
  int inited, initing;

  /*! The current processing phase. */
  enum tm_phase phase, next_phase;

  /*! Possible actions during current phase. */
  int marking;
  int scanning;
  int sweeping;
  int unmarking;

  /*! Global node counts. */
  size_t n[tm__LAST3];

  /*! Number of cumulative allocations during each phase. */
  size_t alloc_by_phase[tm_phase_END];

  /*! Stats during tm_alloc(). */
  size_t alloc_n[tm__LAST3];
  
  /*! Types. */
  /*! List of all types. */
  tm_type types;
  /*! The next type id. */
  int type_id;

  /*! A reserve of tm_type structures. */
  tm_type type_reserve[50], *type_free;
#ifndef tm_type_hash_LEN
#define tm_type_hash_LEN 101
#endif
  /*! Type hash table. */
  tm_type *type_hash[tm_type_hash_LEN];

  /*! Blocks: */

  tm_block *block_first; /* The first block allocated. */
  tm_block *block_last;  /* The last block allocated. */

  /*! A list of free tm_blocks not returned to the OS. */
  tm_list free_blocks;
  /*! Number of tm_blocks in the free_blocks list. */
  int free_blocks_n;

  /*! Block sweeping iterators. */

  /*! Block type. */
  tm_type *bt;
  /*! Block. */
  tm_block *bb;

  /*! OS-level allocation: */

  /*! The address of the last allocation from the operating system. */
  void * os_alloc_last;
  /*! The size of the last allocation from the operating system. */
  size_t os_alloc_last_size;
  /*! The next addresses expected from _tm_os_alloc_().  Only used for sbrk() allocations. */ 
  void * os_alloc_expected; 

  /*! The size of a page-indexed bit map. */
#define tm_BITMAP_SIZE (tm_address_range_k / (tm_page_SIZE / 1024) / bitset_ELEM_BSIZE)

  /*! Valid pointer range. */
  void *ptr_range[2];

  /*! A bit map of pages with nodes in use. */
  bitset_t page_in_use[tm_BITMAP_SIZE];

  /*! A bit map of pages containing allocated block headers. */
  bitset_t block_header[tm_BITMAP_SIZE];

  /*! A bit map of large blocks. */
  bitset_t block_large[tm_BITMAP_SIZE];

  /*! Type color list iterators. */
  tm_node_iterator node_color_iter[tm_TOTAL];

  /*! Partial scan buffer: */

  /*! The current node being scanned for pointers. */
  tm_node *scan_node;
  /*! The current position int the node being scanned for pointers. */
  void *scan_ptr;
  /*! The scan size. */
  size_t *scan_size;

  /*! Roots: */

  /*! List of root regions for scanning. */
  tm_root roots[16];
  /*! Number of roots regions registered. */
  short nroots;

  /*! List of roots to avoid during scanning: anti-roots. */
  tm_root aroots[16]; 
  /*! Number of anti-roots registered. */
  short naroots;

  /*! Huh? */
  short root_datai, root_newi;

  /*! Direction of C stack growth. */
  short stack_grows;

  /*! Pointer to a stack allocated variable.  See user.c. */
  void **stack_ptrp;

  /*! Root being marked. */
  short rooti;
  /*! Current address in root being marked. */
  const char *rp;

  /*! How many root mutations happened since the start of the ROOT phase. */
  unsigned long data_mutations;
  /*! How many stack mutations happened since the start of the ROOT phase. */
  unsigned long stack_mutations;

  /*! Time Stats: */

  /*! Time spent in tm_alloc_os(). */
  tm_time_stat ts_os_alloc;
  /*! Time spent in tm_free_os(). */
  tm_time_stat ts_os_free;
  /*! Time spent in tm_malloc().    */
  tm_time_stat   ts_alloc;
  /*! Time spent in tm_free().     */               
  tm_time_stat   ts_free;
  /*! Time spent in tm_gc_full().  */
  tm_time_stat   ts_gc;                  
  /*! Time spent in tm_write_barrier*(). */
  tm_time_stat   ts_barrier;             
  /*! Time spent in tm_write_barrier*() recoloring black nodes. */
  tm_time_stat   ts_barrier_black;       
  /*! Time spent in each phase during tm_alloc().   */
  tm_time_stat   ts_phase[tm_phase_END]; 
    
  /*! Stats/debugging support: */

  /*! Current allocation id. */
  size_t alloc_id;
  /*! Current allocation pass. */
  size_t alloc_pass;
  /*! Current allocation request size. */
  size_t alloc_request_size;
  /*! Current allocation request type. */
  tm_type *alloc_request_type;

  /*! True if last tm_msg() was disabled; used by tm_msg1() */
  int msg_ignored;
  /*! Table of enabled message types. */
  char msg_enable_table[256];

  /*! Full GC stats: */
  
  /*! */
  size_t blocks_allocated_since_gc;
  /*! */
  size_t blocks_in_use_after_gc;
  /*! */
  size_t nodes_allocated_since_gc;
  /*! */
  size_t nodes_in_use_after_gc;
  /*! */
  size_t bytes_allocated_since_gc;
  /*! */
  size_t bytes_in_use_after_gc;

  /*! File descriptor used by mmap(). */
  int mmap_fd;

  /*! A block of saved CPU registers to root scan. */
  jmp_buf jb;
};


/*! Global data. */
extern struct tm_data tm;

/*! The lowest allocated node address. */
#define tm_ptr_l tm.ptr_range[0]
/*! The highest allocated node address. */
#define tm_ptr_h tm.ptr_range[1]


/*@}*/

/**************************************************************************/
/*! \defgroup node_iterator Node: Iterator */
/*@{*/


/*! Initializes a colored node iterator. */
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

/*! Returns the next node from the iterator. */
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

/*! Initialize the iterator of all nodes of a given color. */
#define tm_node_LOOP_INIT(C) \
  tm_node_iterator_init(&tm.node_color_iter[C])

/*! Begin a loop over all nodes of a given color. */
#define tm_node_LOOP(C) {						\
  tm_node *n;								\
  tm_type *t = 0;							\
  while ( (n = tm_node_iterator_next(&tm.node_color_iter[C])) ) {	\
    t = tm.node_color_iter[C].type;					\
    {

/*! Break out of a loop over all nodes of a given color. */
#define tm_node_LOOP_BREAK(C) break

/*! End a loop over all nodes of a given color. */
#define tm_node_LOOP_END(C)			\
    }						\
  }						\
}


/*@}*/

/****************************************************************************/
/*! \defgroup internal_routine Internal: Routine */
/*@{*/


/*! Initializes a new allocation phase. */
void _tm_phase_init(int p);

/*! Sets the color of a tm_node, in a tm_block. */
void tm_node_set_color(tm_node *n, tm_block *b, tm_color c);

void _tm_set_stack_ptr(void* ptr);

/*! Clears the stack and initializes register root set. */
void __tm_clear_some_stack_words();
#define _tm_clear_some_stack_words() \
setjmp(tm.jb); \
__tm_clear_some_stack_words()


void *_tm_alloc_inner(size_t size);
void *_tm_alloc_desc_inner(tm_adesc *desc);
void *_tm_realloc_inner(void *ptr, size_t size);
void _tm_free_inner(void *ptr);
void _tm_gc_full_inner();

/*@}*/

/****************************************************************************/
/* Internals */

#include "tredmill/os.h"
#include "tredmill/root.h"
#include "tredmill/page.h"
#include "tredmill/ptr.h"
#include "tredmill/mark.h"

/****************************************************************************/
/* Support. */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#endif
