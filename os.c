/** \file os.c
 * \brief Low-level OS interface.
 */
#include "internal.h"

/****************************************************************************/
/*! \defgroup allocation_low_level Allocation: Low-level */
/*@{*/


/*! Total bytes currently allocated from OS. */
size_t tm_os_alloc_total = 0;


#if tm_USE_MMAP
#include <sys/mman.h> 
#endif

/**
 * Allocate memory from the OS.
 *
 * - If tm_USE_MMAP is true, use mmap().
 * - If tm_USE_SBRK is true, use sbrk().
 * .
 */
static 
void *_tm_os_alloc_(long size)
{
  void *ptr;

  tm_assert_test(size > 0);

#if tm_USE_MMAP
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
#endif /* tm_USE_MMAP */

#if tm_USE_SBRK
  ptr = sbrk(size);

  tm.os_alloc_last = ptr;
  tm.os_alloc_last_size = size;

  if ( tm.os_alloc_expected ) {
    tm_warn(ptr == tm.os_alloc_expected, "ptr = %p, expected = %p", ptr, tm.os_alloc_expected);
  }
  if ( size > 0 ) {
    tm.os_alloc_expected = (char *)ptr + size;
  }
#endif /* tm_USE_SBRK */

#if 0
  fprintf(stderr, "  _tm_os_alloc_(%lu) => %p\n", (unsigned long) size, (void*) ptr);
#endif

  return ptr;
}

/**
 * Return memory back to the OS.
 *
 * - If tm_USE_MMAP is true, use munmap().
 * - If tm_USE_SBRK is true, use sbrk(- size) only if the block was the last block requested from sbrk().
 *   Note: other callers to sbrk() may exist.
 * .
 * 
 */
static 
void *_tm_os_free_(void *ptr, long size)
{
#if 0
  fprintf(stderr, "  _tm_os_free_(%p, %lu)\n", (void*) ptr, (unsigned long) size);
#endif

  tm_assert_test(ptr != 0);
  tm_assert_test(size > 0);

#if tm_USE_MMAP
  munmap(ptr, size);

  /*! Return next allocation ptr as "unknown" (0). */
  return 0;
#endif /* tm_USE_MMAP */

#if tm_USE_SBRK
  if ( ptr == tm.os_alloc_last &&
       size == tm.os_alloc_last_size &&
       ptr == sbrk(0) ) {
    ptr = sbrk(- size);

    tm.os_alloc_expected -= size;
  }

  /*! Return next allocation ptr expected (non-0). */
  return ptr;
#endif /* tm_USE_SBRK */
}


/**
 * Allocate memory from OS.
 *
 * - Check soft memory limit.
 * - Update stats and timers.
 */
static __inline
void *_tm_os_alloc(long size)
{
  void *ptr;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_os_alloc);
#endif

  /* Soft memory limit? */
  if ( tm_os_alloc_max && (tm_os_alloc_total + size > tm_os_alloc_max) ) {
    ptr = 0;
  } else {
    ptr = _tm_os_alloc_(size);
  }

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_os_alloc);
#endif

  if ( ptr == 0 || ptr == (void*) -1L ) {
    ptr = 0;
    tm_msg("A 0 %lu\n", (unsigned long) size);
  } else
  if ( size > 0 ) {
    tm_msg("A a %p[%lu] #%lu\n", ptr, (unsigned long) size, (unsigned long) tm.n[tm_B]);

    tm_os_alloc_total += size;
  } else if ( size < 0 ) {
    tm_assert_test(size > 0);
  }
#if 0 
  else {
    tm_msg("A z\n");
  }
#endif

#if 0
  if ( tm.n[tm_B] > 16 ) {
    tm_print_stats();
  }
#endif

  // fprintf(stderr, "  _tm_os_alloc(%lu) => %p\n", (unsigned long) size, (void*) ptr);
  
  return ptr;
}


/**
 * Return block to the OS.
 *
 * - Update stats and timers.
 */
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

  tm_msg("A d %p[%lu] #%lu\n", ptr, (unsigned long) size, (unsigned long) tm.n[tm_B]);

  tm_os_alloc_total -= size;

  return result;
}


/*@}*/

/**************************************************/
/* \defgroup allocation_block_aligned  Allocation: Block-aligned OS */
/*@{*/


/**
 * Allocate a aligned buffer aligned to tm_block_SIZE from the OS.
 */
void *_tm_os_alloc_aligned(size_t size)
{
  void *ptr;

  tm_assert(tm_ptr_is_aligned_to_block(size));

  ptr = _tm_os_alloc(size);

  /*! Return 0 if OS could not allocate a buffer. */
  if ( ! ptr )
    return 0;

  /**
   * Check alignment:
   *
   * If pointer to OS block is not aligned to tm_block_SIZE,
   */
  if ( ! tm_ptr_is_aligned_to_block(ptr) ) {
    size_t offset = ((tm_ptr_word) ptr) % tm_block_SIZE;
    size_t left_over = tm_block_SIZE - offset;
    size_t new_size = size + left_over;

    /**
     *
     * Compute a new size to allow for alignment.
     *
     * <pre>
     *
     * |<--- tm_BLOCK_SIZE --->|<--- tm_BLOCK_SIZE -->|
     * +----------------------------------------------...
     * |<--- offset --->|<--- size      --->|
     * |                |<-lo->|<--- sh --->|
     * +----------------------------------------------...
     *                  ^      ^
     *                  |      |
     *                  ptr    nb
     *
     * </pre>
     *
     */
    tm_msg("A al %p[%lu] %lu %ld\n", (void *) ptr, (unsigned long) size, (unsigned long) tm_block_SIZE, (long) offset);

    // THREAD-RACE on mmap()? {{{

    /*! Give the block back to the OS. */
    _tm_os_free(ptr, size);

    /*! Try again with more room for realignment. */
    ptr = _tm_os_alloc(new_size);

    // THREAD-RACE on mmap()? }}}

    /*! Return 0 if OS would not allocate a bigger buffer. */
    if ( ! ptr )
      return 0;

    /*! Realign to tm_block_SIZE. */
    offset = ((unsigned long) ptr) % tm_block_SIZE;

    tm_msg("A alr %p[%lu] %ld\n", (void *) ptr, (unsigned long) new_size, (long) offset);

    if ( offset ) {
      left_over = tm_block_SIZE - offset;
      
      ptr += left_over;
      new_size -= left_over;

      /*! Assert that alignment is correct. */
      tm_assert(size == new_size);
      tm_assert(tm_ptr_is_aligned_to_block(ptr));
      tm_assert(tm_ptr_is_aligned_to_block(new_size));
    }
  }

  /*! Return aligned ptr. */
  return ptr;
}


/**
 * Free an aligned buffer back to the OS.
 */
void _tm_os_free_aligned(void *ptr, size_t size)
{
  tm_assert_test(tm_ptr_is_aligned_to_block(ptr));
  tm_assert_test(tm_ptr_is_aligned_to_block(size));
 
  _tm_os_free(ptr, size);
}

/*@}*/
