/** \file type.c
 * \brief Type
 */
#include "internal.h"
#include "type.h"


/************************************************************************/
/*! \defgroup type Type */
/*@{*/

/**
 * Initialize a new tm_type of a given size.
 */
void tm_type_init(tm_type *t, size_t size)
{
  /*! Assign a the tm_type a unique id. */
  t->id = ++ tm.type_id;

  /*! Initialize tm_type's global list pointers. */
  tm_list_init(&t->list);
  
  /*! Mark the tm_type as live. */
  tm_list_set_color(&t->list, tm_LIVE_TYPE);
#if tm_name_GUARD
  t->name = "TYPE";
#endif
  t->size = size;

  /*! Initialize the tm_types.blocks list. */
  tm_list_init(&t->blocks);
  tm_list_set_color(&t->blocks, tm_LIVE_BLOCK);

  /*! Initialize the type's tread. */
  tm_tread_init(&t->tread);
  t->tread.c = &tm.colors;

  /*! Initialize the tm_type node counts. */
  t->n = t->tread.n;
  
  /*! Initialize the tm_type.color_list. */
  {
    int j;
    
    for ( j = 0; j < sizeof(t->color_list) / sizeof(t->color_list[0]); ++ j ) {
      tm_list_init(&t->color_list[j]);
      tm_list_set_color(&t->color_list[j], j);
    }
  }

  /*! Force a new tm_block to be allocated and parceled. */
  t->parcel_from_block = 0;

  /*! Zero the tm_type descriptor. */
  t->desc = 0;
}


extern 
int _tm_node_parcel_or_alloc(tm_type *t);

/**
 * Returns a new tm_type for a given size.
 */
tm_type *tm_type_new(size_t size)
{
  tm_type *t;

  /*! If possible take a tm_type from global tm.type_free list. */
  if ( tm.type_free ) {
    t = tm.type_free;
    tm.type_free = t->hash_next;
    t->hash_next = 0;
  } else {
    t = 0;
  }

  /*! Initialize the tm_type. */
  tm_type_init(t, size);

  /*! Add to global tm.types list. */
  tm_list_insert(&tm.types, t);
  
  /**
   * Allocate some tm_nodes now so the tm_type's free list (tm_WHITE) is not empty.
   * to avoid triggering a collection.
   *
   * Assume this is being called because an tm_alloc() is envitable.
   */
  _tm_node_parcel_or_alloc(t);

  return t;
}


/**
 * Returns the tm.type_hash[] index for a size.
 */
static __inline 
int tm_size_hash(size_t size)
{
  int i;

  /*! IMPLEMENT: support for big allocs. */
  tm_assert(size <= tm_block_SIZE_MAX);

  /*! Compute hash bucket index. */
  i = size / tm_ALLOC_ALIGN;

  /*! Modulus index into hash bucket index. */
  i %= tm_type_hash_LEN;

  return i;
}


/**
 * Look for a type by size.
 */
static __inline 
tm_type *tm_size_to_type_2(size_t size)
{
  tm_type **tp, *t;
  int i;
  
  i = tm_size_hash(size);

  /*! Look for type by size using tm.type_hash. */
  for ( tp = &tm.type_hash[i]; (t = (*tp)); tp = &t->hash_next ) {
    /*! If a type of the same size is found, */
    if ( t->size == size ) {
      /* Move it to front of hash bucket. */
      *tp = t->hash_next;
      t->hash_next = tm.type_hash[i];
      tm.type_hash[i] = t;
      break;
    }
  }

  return t;
}


/**
 * Returns a new tm_type for a given size.
 */
static __inline 
tm_type *tm_type_new_2(size_t size)
{
  int i;
  tm_type *t;

  /*! Allocate a new tm_type. */
  t = tm_type_new(size);
  
  /*! Add to tm.type_hash */
  i = tm_size_hash(t->size);
  t->hash_next = tm.type_hash[i];
  tm.type_hash[i] = t;
  
#if 0
  tm_msg("t a t%p %lu\n", (void*) t, (unsigned long) size);
#endif

  return t;
}


/**
 * WHAT DOES THIS DO???
 */
tm_adesc *tm_adesc_for_size(tm_adesc *desc, int force_new)
{
  tm_type *t;

  if ( ! force_new ) {
    t = tm_size_to_type_2(desc->size);
    if ( t )
      return t->desc;
  }

  t = tm_type_new_2(desc->size);
  t->desc = desc;
  t->desc->hidden = t;

  return t->desc;
}


/**
 * Return a tm_type for a size.
 */
tm_type *tm_size_to_type(size_t size)
{
  tm_type *t;
  
  /*! Align size to tm_ALLOC_ALIGN. */
  size = (size + (tm_ALLOC_ALIGN - 1)) & ~(tm_ALLOC_ALIGN - 1);

  /*! If there already exists a type of the size, return it. */
  t = tm_size_to_type_2(size);

  if ( t )
    return t;

  /*! Otherwise, align size to the next power of two. */
#define POW2(i) if ( size <= (1UL << i) ) size = (1UL << i); else

  POW2(3)
  POW2(4)
  POW2(5)
  POW2(6)
  POW2(7)
  POW2(8)
  POW2(9)
  POW2(10)
  POW2(11)
  POW2(12)
  POW2(13)
  POW2(14)
  POW2(15)
  POW2(16)
  (void) 0;

#undef POW2

  /*! Try to locate a bigger tm_type of that size. */
  t = tm_size_to_type_2(size);

  /* If a tm_type was not found, create a new one. */
  if ( ! t ) {
    t = tm_type_new_2(size);
  }
  
  tm_assert_test(t->size == size);
  
  return t;
}


/**
 * Allocates a tm_block of tm_block_SIZE for a tm_type.
 */
tm_block * _tm_type_alloc_block(tm_type *t)
{
  tm_block *b;
  
  /*! Allocate a new tm_block from free list or OS. */
  b = _tm_block_alloc(tm_block_SIZE);

  // fprintf(stderr, "  _tm_block_alloc(%d) => %p\n", tm_block_SIZE, b);

  /*! Add it to the tm_type.blocks list */
  if ( b ) {
    _tm_type_add_block(t, b);
  }

  // tm_msg("b a b%p t%p\n", (void*) b, (void*) t);

  return b;
}


/**
 * Add a tm_block to a tm_type.
 */
void _tm_type_add_block(tm_type *t, tm_block *b)
{
  tm_assert_test(t);
  tm_assert_test(b);
  /*! Assert that tm_block is not already associated with a tm_type. */
  tm_assert_test(b->type == 0);

  /*! Associate tm_block with the tm_type. */
  b->type = t;

  /*! Compute the capacity of this block. */
  tm_assert_test(! b->n[tm_CAPACITY]);
  b->n[tm_CAPACITY] = (b->end - b->begin) / (sizeof(tm_node) + t->size); 

  /*! Begin parceling from this block. */
  tm_assert_test(! t->parcel_from_block);
  t->parcel_from_block = b;

  /*! Increment type block stats. */
  ++ t->n[tm_B];

  /*! Decrement global block stats. */
  ++ tm.n[tm_B];

  /*! Add to type's block list. */
  tm_list_insert(&t->blocks, b);
}


/**
 * Remove a tm_block from its tm_type.
 */
void _tm_type_remove_block(tm_type *t, tm_block *b)
{
  tm_assert_test(t);
  tm_assert_test(b->type);
  tm_assert_test(b->type == t);

  /*! Decrement type block stats. */
  tm_assert_test(t->n[tm_B]);
  -- t->n[tm_B];

  /*! Decrement global block stats. */
  tm_assert_test(tm.n[tm_B]);
  -- tm.n[tm_B];

  /*! Do not parcel nodes from it any more. */
  if ( t->parcel_from_block == b ) {
    t->parcel_from_block = 0;
  }

  /*! Remove tm-block from tm_type.block list. */
  tm_list_remove(b);

  /*! Dissassociate tm_block with the tm_type. */
  b->type = 0;
}


/*@}*/

