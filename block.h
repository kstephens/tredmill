/** \file block.h
 * \brief Block.
 *
 * $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $
 */
#ifndef _tredmill_BLOCK_H
#define _tredmill_BLOCK_H

#include "tredmill/list.h"

/****************************************************************************/
/*! \defgroup block Block */
/*@{*/

/**
 * A block allocated from the operating system.
 *
 * tm_blocks are parceled into tm_nodes of uniform size
 * based on the size of the tm_block's tm_type.
 *
 * All tm_nodes parceled from a tm_block have the same
 * tm_type.
 *
 * A tm_block's address is always aligned to tm_block_SIZE.
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

  /*! The block's id: every block ever allocated from the OS gets a unique id. */
  int id;

  /*! Block's actual size (including this header), multiple of tm_block_SIZE. */
  size_t size;         
  /*! The type the block is assigned to.
   *  All tm_nodes parceled from this block will be of type->size.
   */ 
  struct tm_type *type;
  /*! The beginning of the allocation space. */
  char *begin;

  /*! The end of allocation space.  When alloc reaches end, the block is fully parceled. */
  char *end;

  /*! The next parcel pointer for new tm_nodes.  Starts at begin.  Nodes are parceled by incrementing this attribute. */
  char *next_parcel;

  /**
   * Number of nodes for this block, indexed by tm_color: includes tm_TOTAL.
   * - tm_CAPACITY: capacity of this block in node of this size.
   */
  size_t n[tm_CAPACITY + 1];

#if tm_block_GUARD
  /*! Magic overwrite guard. */
  unsigned long guard2;
#endif

  /*! Force alignment of tm_nodes to double. */
  double alignment[0];
} tm_block;


/*! True if the tm_block has no used nodes; i.e. it can be returned to the OS. */
#define tm_block_unused(b) ((b)->n[WHITE] == b->n[tm_TOTAL])

/*! Returns the type of the block. */
#define tm_block_type(b) ((b)->type)

/*! The begining address of any tm_nodes parcelled from a tm_block. */
#define tm_block_node_begin(b) ((void*) (b)->begin)

/*! The end address of any tm_nodes parcelled from a tm_block. */
#define tm_block_node_end(b) ((void*) (b)->end)

/*! The address of the next tm_node to be parcelled from a tm_block. */
#define tm_block_node_next_parcel(b) ((void*) (b)->next_parcel)

/*! The total size of a tm_node with a useable size based on the tm_block's tm_type size. */
#define tm_block_node_size(b) ((b)->type->size + tm_node_HDR_SIZE)

/*! The adddress of the next tm_node after n, parcelled from tm_block. */
#define tm_block_node_next(b, n) ((void*) (((char*) (n)) + tm_block_node_size(b)))

#if tm_block_GUARD
/*! Validates tm_block for data corruption. */
#define _tm_block_validate(b) do { \
   tm_assert_test(b); \
   tm_assert_test((b)->size); \
   tm_assert_test(! ((void*) &tm <= (void*) (b) && (void*) (b) < (void*) (&tm + 1))); \
   tm_assert_test((b)->guard1 == tm_block_hash(b)); \
   tm_assert_test((b)->guard2 == tm_block_hash(b)); \
   tm_assert_test((b)->begin < (b)->end); \
   tm_assert_test((b)->begin <= (b)->alloc && (b)->alloc <= (b)->end); \
   tm_assert_test((b)->n[WHITE] + (b)->n[ECRU] + (b)->n[GREY] + (b)->n[BLACK] == (b)->n[tm_TOTAL]); \
} while(0)
#else
#define _tm_block_validate(b) ((void) 0)
#endif


void tm_block_init(tm_block *b);

tm_block *_tm_block_alloc_from_free_list(size_t size);
tm_block *_tm_block_alloc(size_t size);
int _tm_block_unparcel_nodes(tm_block *b);
void _tm_block_reclaim(tm_block *b);
void _tm_block_sweep_init();
int tm_block_sweep_some();
void _tm_block_free(tm_block *b);
void tm_block_init_node(tm_block *b, tm_node *n);


/*@}*/

#endif
