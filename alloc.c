#include "internal.h"


/**
 * Allocates a node of a particular size.
 */
void *_tm_alloc_inner(size_t size)
{
  tm_type *type = tm_size_to_type(size);
  tm.alloc_request_size = size;
  tm.alloc_request_type = type;
  return _tm_alloc_type_inner(type);  
}


/**
 * Allocates a node of a particular type.
 */
void *_tm_alloc_desc_inner(tm_adesc *desc)
{
  tm_type *type = (tm_type*) desc->hidden;
  tm.alloc_request_size = type->size;
  tm.alloc_request_type = type;
  return _tm_alloc_type_inner(type);
}


/**
 * Reallocates a node to a particular size.
 */
void *_tm_realloc_inner(void *oldptr, size_t size)
{
  char *ptr = 0;
  tm_type *t, *oldt;

  oldt = tm_node_to_type(tm_pure_ptr_to_node(oldptr));
  t = tm_size_to_type(size);

  if ( oldt == t ) {
    ptr = oldptr;
  } else {
    ptr = _tm_alloc_inner(size);
    memcpy(ptr, oldptr, size < oldt->size ? size : oldt->size);
  }
  
  return (void*) ptr;
}


/*@}*/


