#ifndef tm_PAGE_H
#define tm_PAGE_H

#define tm_ptr_is_aligned_to_block(ptr) !(((unsigned long) ptr) % tm_block_SIZE)
#define tm_ptr_is_aligned_to_page(ptr) !(((unsigned long) ptr) % tm_page_SIZE)

/***************************************************************************/
/* Page bit map mgmt. */

#define tm_page_index(X) (((unsigned long) X) / tm_page_SIZE)

#define tm_page_index_bits (sizeof(tm.page_in_use[0]) * 8)
#define tm_page_index_offset(I) ((I) / tm_page_index_bits)
#define tm_page_index_bit(I) (1U << ((I) % tm_page_index_bits))


static __inline 
int _tm_page_in_use(void *ptr)
{
  unsigned long i = tm_page_index(ptr);
  return 
    tm.page_in_use[tm_page_index_offset(i)] &
    tm_page_index_bit(i);
}


static __inline 
void _tm_page_mark_used(void *ptr)
{
  unsigned long i = tm_page_index(ptr);
  tm.page_in_use[tm_page_index_offset(i)] |=
    tm_page_index_bit(i);
}


static __inline 
void _tm_page_mark_unused(void *ptr)
{
  unsigned long i = tm_page_index(ptr);
  tm.page_in_use[tm_page_index_offset(i)] &=
    ~tm_page_index_bit(i);
}


/* Mark freed pages as unused. */
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


/* Mark freed pages as unused. */
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
