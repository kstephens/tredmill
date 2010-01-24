/** \file os.h
 * \brief Low-level OS interface.
 */
#ifndef tm_OS_H
#define tm_OS_H

/*! If true, use mmap() and munmap(). */
#define tm_USE_MMAP 1
/*! If true, use sbrk(). */
#define tm_USE_SBRK 0

#if tm_USE_MMAP
#undef tm_USE_SBRK
#define tm_USE_SBRK 0
#endif

void *_tm_os_alloc_aligned(size_t size);
void _tm_os_free_aligned(void *ptr, size_t size);

#endif
