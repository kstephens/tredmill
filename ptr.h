#ifndef tm_PTR_H
#define tm_PTR_H


#ifdef  tm_ptr_to_node_TEST
#define tm_ptr_to_node_TEST 0
#endif


/****************************************************************************/
/* Pointer determination. */


static __inline 
tm_type *tm_block_to_type(tm_block *b)
{
  return b->type;
}


static __inline 
tm_block *tm_ptr_to_block(char *p)
{
  char *b;
  size_t offset;

  /*
   * FIXME:
   * This code does not work for blocks bigger than tm_block_SIZE.
   */

  /*
  ** A pointer directly at the end of block should be considered
  ** a pointer into the block before it.
  **
  ** Code like this is more common than expected:
  
  int *p = tm_malloc(sizeof(p[0]) * 10);
  int i = 10;
  while ( -- i >= 0 ) {
    *(p ++) = i;
    some_function_that_calls_tm_malloc(); 
    // when i == 0, p is (just) past end of tm_malloc()ed space and a GC happens.
    }
    p -= 10; // p goes back, p is gone!
  */

  offset = (((unsigned long) p) % tm_block_SIZE);
  b = p - offset;
  if ( offset == 0 ) {
    b -= tm_block_SIZE;
    tm_msg("P bb p%p b%p\n", (void*) p, (void*) b);
    // tm_stop();
  }

  return (void*) b;
}


static __inline 
tm_node *tm_pure_ptr_to_node(void *_p)
{
  // return (tm_node*) (((char*) _p) - tm_node_HDR_SIZE);
  return ((tm_node*) _p) - 1;
}


static __inline 
void *tm_node_to_ptr(tm_node *n)
{
  return (void*) (n + 1);
}


static __inline 
tm_block *tm_node_to_block(tm_node *n)
{
  return tm_ptr_to_block(tm_node_to_ptr(n));
}


static __inline 
tm_node *tm_ptr_to_node(void *p)
{
  tm_block *b;

#if tm_ptr_AT_END_IS_VALID
  /*
  ** A pointer directly at the end of block should be considered
  ** a pointer into the block before it.
  */
  if ( tm_ptr_is_aligned_to_block(p) ) {
    /* This allows _tm_page_in_use(p) to pass. */
    p = p - 1;
  }
#endif

  /* Avoid out of range pointers. */
#if 1
  if ( ! _tm_page_in_use(p) ) 
    return 0;
#else
  if ( ! (tm_ptr_l <= p && p <= tm_ptr_h) )
    return 0;
#endif

  /* Get the block and type. */
  b = tm_ptr_to_block(p);

  _tm_block_validate(b);

  /* Avoid untyped blocks. */
  if ( ! b->type )
    return 0;

  /* Avoid references to block headers or outsize the allocated space. */ 
  if ( p > tm_block_node_alloc(b) )
    return 0;

  if ( p < tm_block_node_begin(b) )
    return 0;

  /* Normalize p to node head by using type size. */
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

#if tm_ptr_AT_END_IS_VALID
      /*
      ** If the pointer is directly after a node boundary
      ** assume it's a pointer to the node before.
      ** 
      ** node0               node1
      ** +---------------...-+---------------...-+...
      ** | tm_node | t->size | tm_node | t->size |
      ** +---------------...-+---------------...-+...
      ** ^                   ^
      ** |                   |
      ** new pp              pp
      */
      if ( node_off == 0 && pp ) {
	pp -= node_size;

	/*
	** Translate back to block header.
	*/
	pp += (unsigned long) b + tm_block_HDR_SIZE;

#if 0	
 	tm_msg("P nb p%p p0%p\n", (void*) p, (void*) pp);
#endif
      } else
#endif

      /*
      ** If the pointer in the node header
      ** it's not a pointer into the node data.
      ** 
      ** node0               node1
      ** +---------------...-+---------------...-+...
      ** | tm_node | t->size | tm_node | t->size |
      ** +---------------...-+---------------...-+...
      **                         ^
      **                         |
      **                         pp
      */
      if ( node_off < tm_node_HDR_SIZE ) {
	return 0;
      }

      /*
      ** Remove intra-node offset.
      ** 
      ** node0               node1
      ** +---------------...-+---------------...-+...
      ** | tm_node | t->size | tm_node | t->size |
      ** +---------------...-+---------------...-+...
      **                     ^            ^
      **                     |            |
      **                     new pp       pp
      */
      else {
	pp -= node_off;

	/*
	** Translate back to block header.
	*/
	pp += (unsigned long) b + tm_block_HDR_SIZE;
      }
    }

    /* It's a node. */
    n = (tm_node*) pp;

    /* Avoid references to free nodes. */
    if ( tm_node_color(n) == tm_WHITE )
      return 0;

    /* Must be okay! */
    return n;
  }
}


static __inline
tm_type *tm_node_to_type(tm_node *n)
{
  tm_block *b = tm_ptr_to_block((char*) n);
  _tm_block_validate(b);
  return b->type;
}


#endif
