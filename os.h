#ifndef tm_OS_H
#define tm_OS_H

void *_tm_os_alloc_aligned(size_t size);
void _tm_os_free_aligned(void *ptr, size_t size);

#endif
