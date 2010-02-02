/** \file internal.h
 * \brief Internals.
 *
 * $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $
 */
#ifndef _tredmill_INTERNAL_H
#define _tredmill_INTERNAL_H

/****************************************************************************/

#include "tm.h"
#include <limits.h> /* PAGESIZE */
#include <sys/time.h> /* struct timeval */
#include <setjmp.h>

#include "tredmill/list.h"
#include "tredmill/config.h"
#include "tredmill/color.h"
#include "tredmill/phase.h"
#include "tredmill/debug.h"
#include "tredmill/log.h"
#include "tredmill/root.h"
#include "tredmill/node.h"
#include "tredmill/block.h"
#include "tredmill/type.h"
#include "tredmill/stats.h"
#include "tredmill/barrier.h"

#include "util/bitset.h" /* bitset_t */


/**************************************************************************/
/*! \defgroup color Color */
/*@{*/

/*
This table defines what color a newly allocated node
should be during the current allocation phase.
*/

/**
 * Default allocated nodes to "not marked".
 *
 * Rational: some are allocated but thrown away early.
 */
#define tm_DEFAULT_ALLOC_COLOR tm_ECRU

/**
 * Default allocated node color during sweep.
 *
 * Rational: 
 * tm_ECRU is considered garabage during tm_SWEEP.
 *
 * Assume that new nodes are actually in-use and may contain pointers 
 * to prevent accidental sweeping.
 */
#define tm_SWEEP_ALLOC_COLOR tm_GREY

/*@}*/


/****************************************************************************/
/*! \defgroup internal_data Internal: Data */
/*@{*/

/**
 * Internal data for the allocator.
 *
 * This structure is specifically avoided by the uninitialized
 * data segment root, using an anti-root.  See tm_init().
 *
 * FIXME: this structure is very large due to the statically allocated block bit maps.
 */
struct tm_data {
  /*! Current status. */
  int inited, initing;

  /*! The phase data. */
  tm_phase_data p;

  /*! The current colors. */
  tm_colors colors;

  /*! The color of newly allocated nodes. */
  int alloc_color;

  /*! Number of transitions from one color to another. */
  size_t n_color_transitions[tm_TOTAL + 1][tm_TOTAL + 1];

 /*! If true, full GC is triggered on next call to tm_alloc(). */
  int trigger_full_gc;

  /*! Global node counts. */
  size_t n[tm__LAST3];

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

  /*! The next block id. */
  int block_id;

  /*! The first block allocated. */
  tm_block *block_first;
  /*! The last block allocated. */
  tm_block *block_last;

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

  /*! How many root mutations happened since root scan. */
  unsigned long data_mutations;
  /*! How many stack mutations happened since root scan. */
  unsigned long stack_mutations;

  /*! Time Stats: */

  /*! Time spent in tm_alloc_os(). */
  tm_time_stat   ts_os_alloc;
  /*! Time spent in tm_free_os(). */
  tm_time_stat   ts_os_free;
  /*! Time spent in tm_malloc().    */
  tm_time_stat   ts_alloc;
  /*! Time spent in tm_free().     */               
  tm_time_stat   ts_free;
  /*! Time spent in tm_gc_full().  */
  tm_time_stat   ts_gc;                  
  /*! Time spent in tm_gc_full_inner().  */
  tm_time_stat   ts_gc_inner;                  
  /*! Time spent in tm_write_barrier(). */
  tm_time_stat   ts_barrier;             
  /*! Time spent in tm_write_barrier_root(). */
  tm_time_stat   ts_barrier_root;       
  /*! Time spent in tm_write_barrier_pure(). */
  tm_time_stat   ts_barrier_pure;       
  /*! Time spent in tm_write_barrier on tm_BLACK nodes. */
  tm_time_stat   ts_barrier_black;
    
  /*! Stats/debugging support: */

  /*! Current allocation id. */
  size_t alloc_id;

  /*! Current allocation pass. */
  size_t alloc_pass;
  
  /*! Allocations since sweep. */
  size_t alloc_since_sweep;

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
  ni->scan_node = 0;
  ni->scan_ptr = 0;
  ni->scan_end = 0;
  ni->scan_size = 0;
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

      /*
       * There are no tm_type objects at all.
       * This should never happen!
       */
      if ( (void *) ni->type == (void *) &tm.types ) {
	tm_abort();
	return 0;
      }
      
    next_type:
      /* Start on type node color_list. */
      ni->node_next = (void *) &ni->type->color_list[ni->color];

      tm_assert(tm_list_color(ni->node_next) == ni->color);

      /* Move iterator to first node. */
      ni->node_next = (void *) tm_list_next(ni->node_next);
    }

    /* At end of type color list? */
    if ( ! ni->node_next || (void *) ni->node_next == (void *) &ni->type->color_list[ni->color] ) {
      ni->type = (void*) tm_list_next(ni->type);
      goto next_type;
    }

    if ( tm_node_color(ni->node_next) != ni->color ) {
      fprintf(stderr, "  tm_node_iterator %p: node_color_iter[%s] derailed at node_next %p color %s, t#%d\n",
	      ni, 
	      tm_color_name[ni->color], 
	      ni->node_next, 
	      tm_color_name[tm_node_color(ni->node_next)],
	      ni->type->id
	      );
      // tm_abort();
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
