/** \file page.h
 * \brief Page-oriented bit maps.
 */
#ifndef tm_PAGE_H
#define tm_PAGE_H

#include "util/bitset.h"
#include "tredmill/tm_data.h"


/*! True if ptr is aligned to a tm_block */
#define tm_ptr_is_aligned_to_block(ptr) !(((unsigned long) ptr) % tm_block_SIZE)

/*! True if ptr is aligned to a OS page. */
#define tm_ptr_is_aligned_to_page(ptr) !(((unsigned long) ptr) % tm_page_SIZE)


/***************************************************************************/


/*! Returns the index of a ptr into page-orientated bit map */ 
#define tm_page_index(X) (((unsigned long) X) / tm_page_SIZE)


/**
 * Is page at ptr in use?
 */
static __inline 
int _tm_page_in_use(void *ptr)
{
  size_t i = tm_page_index(ptr);
  return bitset_get(tm.page_in_use, i);
}

/**
 * Mark page at ptr in use.
 */
static __inline 
void _tm_page_mark_used(void *ptr)
{
  size_t i = tm_page_index(ptr);
  bitset_set(tm.page_in_use, i);
}


/**
 * Mark page at ptr unused.
 */
static __inline 
void _tm_page_mark_unused(void *ptr)
{
  size_t i = tm_page_index(ptr);
  bitset_clr(tm.page_in_use, i);
}


/**
 * Mark freed pages at ptr for size bytes as used.
 */
static __inline 
void _tm_page_mark_used_range(void *ptr, size_t size)
{
  tm_assert_test(tm_ptr_is_aligned_to_page(ptr));
  tm_assert_test(tm_ptr_is_aligned_to_page(size));

  while ( size > 0 ) {
    _tm_page_mark_used(ptr);
    ptr += tm_page_SIZE;
    size -= tm_page_SIZE;
  }
}


/**
 * Mark freed pages at ptr for size bytes as unused.
 */
static __inline 
void _tm_page_mark_unused_range(void *ptr, size_t size)
{
  tm_assert_test(tm_ptr_is_aligned_to_page(ptr));
  tm_assert_test(tm_ptr_is_aligned_to_page(size));

  while ( size > 0 ) {
    _tm_page_mark_unused(ptr);
    ptr += tm_page_SIZE;
    size -= tm_page_SIZE;
  }
}

#endif
