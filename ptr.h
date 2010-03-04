/** \file ptr.h
 * \brief Mapping potential pointers to internal structures.
 */
#ifndef tm_PTR_H
#define tm_PTR_H

#include "tredmill/page.h"  /* _tm_page_in_use() */

#ifdef  tm_ptr_to_node_TEST
#define tm_ptr_to_node_TEST 0
#endif


/****************************************************************************/
/*! \defgroup pointer */
/*@{*/

/*! Returns the tm_type of a tm_block. */
static __inline 
tm_type *tm_block_to_type(tm_block *b)
{
  return b->type;
}


/**
 * Returns the potential tm_block of a potential pointer.
 *
 * FIXME: This code does not work for blocks bigger than tm_block_SIZE.
 */
static __inline 
tm_block *tm_ptr_to_block(char *p)
{
  /**
   * If tm_ptr_AT_END_IS_VALID is true,
   * A pointer directly at the end of tm_block should be considered
   * a pointer into the tm_block before it,
   * because there may be a node allocated contigiously before
   * tm_block header.
   *
   * Example:
   * 
   * <pre>
   *
   * ... ----------------+----------------------------- ... 
   *          tm_block a | tm_block b                   
   *     |  node | node  | tm_block HDR | node | node | 
   * ... ----------------+------------------------------... 
   *             ^       ^
   *             |       |
   *             p!      p?
   *
   * </pre>
   *
   * Code behaving like this might be pathological, but is valid:
   * 
   * <pre>
   * 
   *   int *p = tm_alloc(sizeof(p[0]) * 10);
   *   int i = 10;
   *   while ( -- i >= 0 ) {
   *     *(p ++) = i;
   *     some_function_that_calls_tm_alloc(); 
   *     //
   *     // when i == 0, p is (just) past end of tm_alloc() data space and a GC happens.
   *     // There are no live pointers to the original tm_alloc() data space. 
   *     //
   *   }
   *   p -= 10; 
   *   // p goes back original tm_alloc() data space, 
   *   // but p has been reclaimed!
   *
   * </pre>
   */

#if tm_ptr_AT_END_IS_VALID
  char *b; /*!< The pptr aligned to the tm_block_SIZE. */
  size_t offset; /*! The offset of the pptr in a tm_block. */

  /*! Find the offset of p in an aligned tm_block. */
  offset = (((unsigned long) p) % tm_block_SIZE);
  b = p - offset;
  /*! If the pptr is directly at the beginning of the block, */
  if ( offset == 0 && p ) {
    /*! Assume its in the previous block, by subtracting tm_block_SIZE. */
    b -= tm_block_SIZE;
    tm_msg("P bb p%p b%p\n", (void*) p, (void*) b);
    // tm_stop();
  }

  return (void*) b;
#else
  /**
   * If tm_ptr_AT_END_IS_VALID is false,
   * Mask off the tm_block_SIZE bits from the address.
   */
  return (void*) ((unsigned long) p & tm_block_SIZE_MASK);
#endif /* tm_ptr_AT_END_IS_VALID */
}


/**
 * Returns the tm_node of a pointer.
 *
 * Subtracts the tm_node header size from the pointer.
 */
static __inline 
tm_node *tm_pure_ptr_to_node(void *_p)
{
  // return (tm_node*) (((char*) _p) - tm_node_HDR_SIZE);
  return ((tm_node*) _p) - 1;
}


/**
 * Returns the data pointer of a tm_node.
 *
 * Adds the tm_node header size to the tm_node address.
 */
static __inline 
void *tm_node_to_ptr(tm_node *n)
{
  return (void*) (n + 1);
}


/**
 * Returns the tm_block of a tm_node.
 */
static __inline 
tm_block *tm_node_to_block(tm_node *n)
{
  return tm_ptr_to_block(tm_node_to_ptr(n));
}


/**
 * Returns the tm_node of a potential pointer.
 */
static __inline 
tm_node *tm_ptr_to_node(void *p)
{
  tm_block *b;

#if 0
  /*! If p is 0, it is not a pointer to a tm_node. */
  if ( ! p )
    return 0;
#endif

  /**
   * If tm_ptr_AT_END_IS_VALID is true,
   * A pointer directly at the end of block should be considered
   * a pointer into the block before it.
   * See tm_ptr_to_block().
   */
#if tm_ptr_AT_END_IS_VALID
  if ( tm_ptr_is_aligned_to_block(p) ) {
    /*! This allows _tm_page_in_use(p) to pass. */
    p = p - 1;
  }
#endif

#if 1
  /*! Avoid pointers into pages not marked in use. */
  if ( ! _tm_page_in_use(p) ) 
    return 0;
#else
  /*! Avoid out of range pointers. */
  if ( ! (tm_ptr_l <= p && p <= tm_ptr_h) )
    return 0;
#endif

  /*! Get the block and type. */
  b = tm_ptr_to_block(p);

  _tm_block_validate(b);

  /*! Avoid untyped blocks. */
  if ( ! b->type )
    return 0;

  /*! Avoid references to block headers or outside the allocated space. */ 
  if ( p >= tm_block_node_next_parcel(b) )
    return 0;

  if ( p < tm_block_node_begin(b) )
    return 0;

  /*! Normalize p to node head by using its tm_type size. */
  {
    unsigned long pp = (unsigned long) p;
    unsigned long node_size = tm_block_node_size(b);
    tm_node *n;
    
    /*
    ** Translate away the block header.
    */
    pp -= (unsigned long) b + tm_block_HDR_SIZE;


    {
      unsigned long node_off = pp % node_size;

      /**
       * If tm_ptr_AT_END_IS_VALID is true,
       * If the pointer is directly after a node boundary
       * assume it's a pointer to the node before.
       *
       * <pre>
       *
       * node0               node1
       * +---------------...-+---------------...-+...
       * | tm_node | t->size | tm_node | t->size |
       * +---------------...-+---------------...-+...
       * ^                   ^
       * |                   |
       * new pp              pp
       *
       * </pre>
       *
       * Translate the pointer back to the tm_block header.
       */
#if tm_ptr_AT_END_IS_VALID
      if ( node_off == 0 && pp ) {
	pp -= node_size;
	pp += (unsigned long) b + tm_block_HDR_SIZE;

#if 0	
 	tm_msg("P nb p%p p0%p\n", (void*) p, (void*) pp);
#endif
      } else
#endif

	/**
	 * If the pointer is in the node header,
	 * it's not a pointer into the node data.
	 * 
	 * <pre>
	 * node0               node1
	 * +---------------...-+---------------...-+...
	 * | tm_node | t->size | tm_node | t->size |
	 * +---------------...-+---------------...-+...
	 *                         ^
	 *                         |
	 *                         pp
	 * </pre>
	 *
	 */
      if ( node_off < tm_node_HDR_SIZE ) {
	return 0;
      }

      /**
       * Remove intra-node offset.
       *
       * <pre>
       *
       * node0               node1
       * +---------------...-+---------------...-+...
       * | tm_node | t->size | tm_node | t->size |
       * +---------------...-+---------------...-+...
       *                     ^            ^
       *                     |            |
       *                     new pp       pp
       *
       * </pre>
       *
       */
      else {
	pp -= node_off;

	/**
	 * Translate back to block header.
	 */
	pp += (unsigned long) b + tm_block_HDR_SIZE;
      }
    }

    /*! It's a node. */
    n = (tm_node*) pp;

    /*! Avoid references to free nodes. */
    if ( tm_node_color(n) == WHITE )
      return 0;

    /*! Return a node. */
    return n;
  }
}


/**
 * Returns the tm_type of a tm_node.
 */
static __inline
tm_type *tm_node_to_type(tm_node *n)
{
  tm_block *b = tm_ptr_to_block((char*) n);
  _tm_block_validate(b);
  return b->type;
}

/*@}*/

#endif
