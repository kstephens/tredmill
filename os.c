#include "internal.h"

/****************************************************************************/
/* Low-level alloc. */


size_t tm_os_alloc_total = 0;


#define USE_MMAP 1
#define USE_SBRK 0


#if USE_MMAP

#include <sys/mman.h> 

static 
void *_tm_os_alloc_(long size)
{
  void *ptr;

  tm_assert(size > 0);

#ifdef MAP_ANONYMOUS
  tm.mmap_fd = -1;
#endif

  ptr = mmap((void*) 0,
	     (size_t) size,
	     PROT_READ | PROT_WRITE,
#ifdef MAP_ANONYMOUS
	     MAP_ANONYMOUS |
#endif
	     MAP_PRIVATE,
	     tm.mmap_fd,
	     (off_t) 0
	     );

  if ( ptr == MAP_FAILED ) {
    perror("tm: mmap() failed");
    ptr = 0;
    tm_abort();
  }
 
  fprintf(stderr, "  _tm_os_alloc_(%ld) => %p\n", (long) size, (void*) ptr);

  return ptr;
}

static 
void *_tm_os_free_(void *ptr, long size)
{
  fprintf(stderr, "  _tm_os_free_(%p, %ld)\n", (void*) ptr, (long) size);

  munmap(ptr, size);

  /* next allocation ptr unknown. */
  return 0;
}

#endif


#if USE_SBRK

static __inline
void *_tm_os_alloc_(long size)
{
  void *ptr;

  tm_assert(size > 0);

  ptr = sbrk(size);

  tm.os_alloc_last = ptr;
  tm.os_alloc_last_size = size;

  if ( tm.os_alloc_expected ) {
    tm_warn(ptr == tm.os_alloc_expected, "ptr = %p, expected = %p", ptr, tm.os_alloc_expected);
  }
  if ( size > 0 ) {
    tm.os_alloc_expected = (char *)ptr + size;
  }

  return ptr;
}

static 
void _tm_os_free_(void *ptr, long size)
{
  void *ptr = 0;

  tm_assert(ptr != 0);
  tm_assert(size > 0);

  if ( ptr == tm.os_alloc_last &&
       size == tm.os_alloc_last_size ) {
    ptr = sbrk(- size);

    tm.os_alloc_expected -= size;
  }

  return ptr;
}

#endif


static __inline
void *_tm_os_alloc(long size)
{
  void *ptr;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_os_alloc);
#endif

  /* Soft memory limit? */
  if ( tm_os_alloc_total + size > tm_os_alloc_max ) {
    ptr = 0;
  } else {
    ptr = _tm_os_alloc_(size);
  }

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_os_alloc);
#endif

  if ( ptr == 0 || ptr == (void*) -1L ) {
    ptr = 0;
    tm_msg("A 0 %ld\n", (long) size);
  } else
  if ( size > 0 ) {
    tm_msg("A a %p[%ld]\n", ptr, (long) size);

    tm_os_alloc_total += size;
  } else if ( size < 0 ) {
    tm_assert(size > 0);
  }
#if 0 
  else {
    tm_msg("A z\n");
  }
#endif

  // fprintf(stderr, "  _tm_os_alloc(%ld) => %p\n", (long) size, (void*) ptr);
  
  return ptr;
}


static __inline
void *_tm_os_free(void *ptr, long size)
{
  void *result = 0;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_os_free);
#endif

  result = _tm_os_free_(ptr, size);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_os_free);
#endif

  /* Mark freed pages as unused. */
  if ( tm_ptr_is_aligned_to_page(ptr) ) {
    _tm_page_mark_unused_range(ptr, size);
  }

  tm_msg("A d %p[%ld]\n", ptr, (long) size);

  tm_os_alloc_total -= size;

  return result;
}


/**************************************************/
/* Block-aligned OS allocation */


void *_tm_os_alloc_aligned(size_t size)
{
  void *ptr;

  tm_assert(tm_ptr_is_aligned_to_block(size));

  ptr = _tm_os_alloc(size);

  if ( ! ptr )
    return 0;

  /* Check alignment. */
  if ( ! tm_ptr_is_aligned_to_block(ptr) ) {
    size_t offset = ((unsigned long) ptr) % tm_block_SIZE;
    size_t left_over = tm_block_SIZE - offset;
    size_t new_size = size + left_over;

    /*
    ** |<--- tm_BLOCK_SIZE --->|<--- tm_BLOCK_SIZE -->|
    ** +----------------------------------------------...
    ** |<--- offset --->|<--- size      --->|
    ** |                |<-lo->|<--- sh --->|
    ** +----------------------------------------------...
    **                  ^      ^
    **                  |      |
    **                  ptr    nb
    */
    tm_msg("A al %p[%lu] %ld %ld\n", (void *) ptr, (long) size, (long) tm_block_SIZE, (long) offset);

    // THREAD-RACE on mmap()? {{{

    /* Give it back to the OS. */
    _tm_os_free(ptr, size);

    /* Try again with more room for realignment. */
    ptr = _tm_os_alloc(new_size);

    // THREAD-RACE on mmap()? }}}

    if ( ! ptr )
      return 0;

    offset = ((unsigned long) ptr) % tm_block_SIZE;

    tm_msg("A alr %p[%lu] %ld\n", (void *) ptr, (long) new_size, (long) offset);

    if ( offset ) {
      left_over = tm_block_SIZE - offset;
      
      ptr += left_over;
      new_size -= left_over;
      
      tm_assert(size == new_size);
      tm_assert(tm_ptr_is_aligned_to_block(ptr));
      tm_assert(tm_ptr_is_aligned_to_block(new_size));
    }
  }

  return ptr;
}


void _tm_os_free_aligned(void *ptr, size_t size)
{
  tm_assert(tm_ptr_is_aligned_to_block(ptr));
  tm_assert(tm_ptr_is_aligned_to_block(size));
 
  _tm_os_free(ptr, size);
}

