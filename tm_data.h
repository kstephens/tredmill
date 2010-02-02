#ifndef _tm_TM_DATA_H
#define _tm_TM_DATA_H

#include <setjmp.h>

#include "tredmill/config.h"

#include "util/bitset.h" /* bitset_t */

#include "tredmill/color.h"
#include "tredmill/phase.h"
#include "tredmill/type.h"
#include "tredmill/block.h"
#include "tredmill/root.h"
#include "tredmill/node.h"


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

#if 0
  /*! The phase data. */
  struct tm_phase_data p;
#endif

  /*! The current colors. */
  struct tm_colors colors;

  /*! The color of newly allocated nodes. */
  int alloc_color;

  /*! If true, nodes are currently being parceled. */
  int parceling;

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
  tm_list types;

  /*! The next type id. */
  int type_id;

  /*! Type scanning pointer. */
  tm_type *type_scan;

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
  /*! Time spent in tm_write_barrier on BLACK nodes. */
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


#define WHITE tm.colors.c[tm_WHITE]
#define ECRU  tm.colors.c[tm_ECRU]
#define GREY  tm.colors.c[tm_GREY]
#define BLACK tm.colors.c[tm_BLACK]


/*@}*/



#endif
