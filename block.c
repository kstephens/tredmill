/** \file block.c
 * \brief Block
 *
 * - $Id: tm.c,v 1.20 2009-08-01 10:47:31 stephens Exp $
 */

#include "internal.h"


/**************************************************/
/*! \defgroup block Block */
/*@{*/


/**
 * Initialize a new tm_block.
 */
void tm_block_init(tm_block *b)
{
  tm_assert_test(b->size);

  /*! Initialize the block id. */
  if ( ! b->id ) {
    b->id = ++ tm.block_id;
  }

  /*! Initialize tm_block list pointers. */
  tm_list_init(&b->list);
  /*! Mark tm_block as tm_LIVE_BLOCK. */
  tm_list_set_color(b, tm_LIVE_BLOCK);

#if tm_name_GUARD
  b->name = "BLOCK";
#endif

  /*! Disassocate the tm_block with any tm_type. */
  b->type = 0;

  /*! Initialize bounds of the tm_block's allocation space. */
  b->begin = (char*) b + tm_block_HDR_SIZE;
  b->end   = (char*) b + b->size;
  
  /*! Initialize next tm_node parcel to beginning of valid useable space. */
  b->next_parcel = b->begin;

#if tm_block_GUARD
  b->guard1 = b->guard2 = tm_block_hash(b);
#endif

  /*! Clear tm_block stats. */
  memset(b->n, 0, sizeof(b->n));

  /*! Reset tm_block sweep iterator. */
  _tm_block_sweep_init();

  /*! Remember the first block and most recent blocks allocated. */
  tm.block_last = b;
  if ( ! tm.block_first ) {
    tm.block_first = b;
  } else {
#if tm_USE_SBRK
    /* Make sure heap grows up, other code depends on it. */
    tm_assert((void*) b >= (void*) tm.block_first);
#endif
  }
}


/**
 * Align size to a multiple of tm_block_SIZE.
 */
static __inline
size_t tm_block_align_size(size_t size)
{
  size_t offset;

  /*! Force allocation to a multiple of tm_block_SIZE. */
  if ( (offset = (size % tm_block_SIZE)) )
    size += tm_block_SIZE - offset;

  return size;
}


/**
 * Allocate a tm_block from the free list.
 * 
 * Force allocation to a multiple of tm_block_SIZE.
 */
tm_block *_tm_block_alloc_from_free_list(size_t size)
{
  tm_block *b = 0;

  size = tm_block_align_size(size);

  /*! Scan global tm_block free list for a block of the right size. */
  {
    tm_block *bi;

    tm_list_LOOP(&tm.free_blocks, bi);
    {
      if ( bi->size == size &&
	   tm_list_color(bi) == tm_FREE_BLOCK
	   ) {
	tm_assert_test(tm.free_blocks_n);
	-- tm.free_blocks_n;

	tm_list_remove(bi);

	b = bi;

	tm_msg("b a fl b%p %d\n", (void*) b, tm.free_blocks_n);
	break;
      }
    }
    tm_list_LOOP_END;
  }
  
  if ( b ) {
    /*! Initialize the tm_block. */
    tm_block_init(b);
    
    /*! Increment global block stats. */
    ++ tm.n[tm_B];
    tm.blocks_allocated_since_gc += size / tm_block_SIZE;
  }

  /*! Return 0 if no free blocks exist. */
  return b;
}


/**
 * Allocate a tm_block of a given size.
 */
tm_block *_tm_block_alloc(size_t size)
{
  tm_block *b;

  size = tm_block_align_size(size);

  /*! Try to allocate from block free list. */
  b = _tm_block_alloc_from_free_list(size);

  /** 
   * Otherwise, allocate a new tm_block from the OS.
   */
  if ( ! b ) {
    /*! Make sure it's aligned to tm_block_SIZE. */
    b = _tm_os_alloc_aligned(size);

    /*! Return 0 if OS denied. */
    if ( ! b )
      return 0;

    /*! Force allocation of a new block id. */
    b->id = 0;

    /*! Initialize its size. */
    b->size = size;
    
    /*! Increment OS block stats. */
    ++ tm.n[tm_B_OS];
    if ( tm.n[tm_B_OS_M] < tm.n[tm_B_OS] )
      tm.n[tm_B_OS_M] = tm.n[tm_B_OS];

    tm.n[tm_b_OS] += size;
    if ( tm.n[tm_b_OS_M] < tm.n[tm_b_OS] )
      tm.n[tm_b_OS_M] = tm.n[tm_b_OS];

    tm_msg("b a os b%p\n", (void*) b);

    /*! Initialize the tm_block. */
    tm_block_init(b);
    
    /*! Increment global block stats. */
    ++ tm.n[tm_B];
    tm.blocks_allocated_since_gc += size / tm_block_SIZE;
  }

  // tm_validate_lists();

  // fprintf(stderr, "  _tm_block_alloc(%p)\n", b);

  /*! Return the new or reused tm_block. */
  return b;
}


/**
 * Begin sweeping of tm_blocks.
 *
 * UNIMPLEMENTED!
 */
void _tm_block_sweep_init()
{
#if 0
  tm.bt = tm_list_next(&tm.types);
  tm.bb = tm.bt != tm_list_next(tm.bt) ?
    tm_list_next(&tm.bt->blocks) :
    0;
#endif
}


#if 0
/**
 * Scavenges all tm_types for an unused tm_block.
 *
 * NOT USED!
 */
tm_block *tm_block_scavenge(tm_type *t)
{
  tm_type *type = 0;

  tm_list_LOOP(&tm.types, type);
  {
    tm_block *block = 0;

    tm_list_LOOP(&type->blocks, block);
    {
      if ( block->type != t && 
	   tm_block_unused(block) &&
	   block->n[tm_TOTAL]
	   ) {
	return block;
      }
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;

  return 0;
}
#endif


/**********************************************************/
/* Block free */

/**
 * Deletes a WHITE tm_node from a tm_block.
 */
static __inline
void _tm_block_delete_node(tm_block *b, tm_node *n)
{
  tm_type *t;
  tm_color nc = tm_node_color(n);

  // _tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;
  tm_assert_test(nc == tm_WHITE);

  /*! Remove tm_node from tm_type's color list. */
  tm_list_remove(n);

  // tm_validate_lists();

#if 0
  tm_msg("N d n%p[%lu] t%p\n", 
	 (void*) n,
	 (unsigned long) t->size,
	 (void*) t);
#endif
}


/**
 * Unparcels the tm_nodes in a tm_block.
 *
 * - Removes each tm_node allocated from the tm_block from
 * its WHITE list.
 * - All tm_nodes in the tm_block must be WHITE.
 */
int _tm_block_unparcel_nodes(tm_block *b)
{
  int count = 0, bytes = 0;
  tm_type *t = b->type;
  
  tm_assert_test(b->type);

  /**
   * If all nodes in the block are free, 
   * remove all nodes in the block from any lists.
  */
  tm_assert_test(tm_block_unused(b));

  {
    tm_node *n;
    
    /*! Start at first tm_node in tm_block. */
    n = tm_block_node_begin(b);
    while ( (void*) n < tm_block_node_next_parcel(b) ) {
      /*! Remove node from tm_WHITE list and advance. */
      ++ count;
      bytes += b->size;

      tm_assert_test(tm_node_color(n) == tm_WHITE);
      _tm_block_delete_node(b, n);

      n = tm_block_node_next(b, n);
    }
  }

  /*! Decrement tm_WHITE and tm_TOTAL counts: */

  /*! - Decrement type node counts. */
  tm_assert_test(t->n[tm_WHITE] >= count);
  t->n[tm_WHITE] -= count;
  tm_assert_test(t->n[tm_TOTAL] >= count);
  t->n[tm_TOTAL] -= count;

  /*! - Decrement block node counts. */
  tm_assert_test(b->n[tm_WHITE] >= count);
  b->n[tm_WHITE] -= count;
  tm_assert_test(b->n[tm_WHITE] == 0);

  tm_assert_test(b->n[tm_TOTAL] >= count);
  b->n[tm_TOTAL] -= count;
  tm_assert_test(b->n[tm_TOTAL] == 0);

  /*! - Decrement global node counts. */
  tm_assert_test(tm.n[tm_WHITE] >= count);
  tm.n[tm_WHITE] -= count;
  tm_assert_test(tm.n[tm_TOTAL] >= count);
  tm.n[tm_TOTAL] -= count;

  return count;
}


/**
 * Reclaim a live tm_block.
 */
void _tm_block_reclaim(tm_block *b)
{
  tm_assert_test(b);
  tm_assert_test(tm_list_color(b) == tm_LIVE_BLOCK);

  /*! Unparcel any allocated nodes from type free lists. */
  _tm_block_unparcel_nodes(b);

  /*! Avoid pointers into block. */
  b->next_parcel = b->begin;

  /*! Decrement global block stats. */
  tm_assert_test(tm.n[tm_B]);
  -- tm.n[tm_B];

  /*! Remove reference from tm.block_first and tm.block_last, if necessary. */
  if ( tm.block_last == b ) {
    tm.block_last = 0;
  }
  if ( tm.block_first == b ) {
    tm.block_first = 0;
  }

  /*! Remove tm_block from tm_type. */
  _tm_type_remove_block(b->type, b);

  /*! Mark tm_block's pages as unused. */
  _tm_page_mark_unused_range(b, b->size);
}


/**
 * Frees a block either returning the block to the OS or keeping it on a free list.
 *
 * - If using mmap(), the block is returned to the OS only if there are enough blocks on the free list (see: tm_block_min_free).
 * - If using sbrk(), the block is returned to the OS only if it was the most recient block allocated by sbrk().
 *
 */
void _tm_block_free(tm_block *b)
{
  int os_free = 0;

  tm_assert(tm_block_unused(b));

  /*! Reclaim tm_block from its tm_type.blocks list. */
  _tm_block_reclaim(b);

#if tm_USE_MMAP
  /*! If using mmap(), reduce calls to _tm_os_free() by keeping tm_block_min_free free blocks. */
  if ( tm.free_blocks_n > tm_block_min_free ) {
    // fprintf(stderr, "  tm_block_free too many free blocks: %d\n", tm.free_blocks_n);
    os_free = 1;
  } else {
    os_free = 0;
  }
#endif

#if tm_USE_SBRK
  /*! If using sbrk() and b was the last block allocated from the OS, */
  if ( sbrk(0) == (void*) b->end ) {
    /*! Reduce valid node ptr range. */
    tm_assert(tm_ptr_h == b->end);
    tm_ptr_h = b;

    /*! And plan to return block to OS. */
    os_free = 1;
  } else {
    os_free = 0;
  }
#endif

  /*! If block should return to OS, */
  if ( os_free ) {
    /*! Decrement global OS block stats. */
    tm_assert_test(tm.n[tm_B_OS]);
    -- tm.n[tm_B_OS];

    tm_assert_test(tm.n[tm_b_OS] > b->size);
    tm.n[tm_b_OS] -= b->size;

    b->id = 0;

    /*! And return aligned block back to OS. */
    _tm_os_free_aligned(b, b->size);

    tm_msg("b f os b%p\n", (void*) b);
  } else {
    /*! Otherwise, remove from t->blocks list and add to global free block list. */
    tm_list_remove_and_append(&tm.free_blocks, b);
    ++ tm.free_blocks_n;

    /*! Mark block as tm_FREE_BLOCK. */
    tm_list_set_color(b, tm_FREE_BLOCK);

    tm_msg("b f fl b%p %d\n", (void*) b, tm.free_blocks_n);
  }

  /*! Reset block sweep iterator. */
  _tm_block_sweep_init();

  // tm_validate_lists();

  // fprintf(stderr, "  _tm_block_free(%p)\n", b);
  // tm_msg("b f b%p\n", (void*) b);

  /*! Return. */
}


/**
 * Initialize a tm_node from a tm_block.
 */
void tm_block_init_node(tm_block *b, tm_node *n)
{
  tm_type *t;

  tm_assert_test(b);
  // _tm_block_validate(b);
  tm_assert_test(b->type);

  /*! Set the tm_block.type. */
  t = b->type;

  /*! Initialize its list pointers. */
  tm_list_init(&n->list);

#if 1
  tm_assert_test(tm_list_color(&n->list) == tm_WHITE);
#else
  /*! Set the tm_node color to tm_WHITE. */
  tm_list_set_color(n, tm_WHITE);
#endif

  /*! Increment type node counts. */
  ++ t->n[tm_TOTAL];
  ++ t->n[tm_WHITE];

  /*! Increment block node counts */
  ++ b->n[tm_TOTAL];
  ++ b->n[tm_WHITE];
    
  /*! Increment global node counts. */
  ++ tm.n[tm_TOTAL];
  ++ tm.n[tm_WHITE];
  
  /*! Place tm_node on tm_type tm_WHITE list. */
  tm_node_set_color(n, b, tm_WHITE);
  
  // tm_validate_lists();

  // tm_msg("N n%p t%p\n", (void*) n, (void*) t);
}


/*@}*/

