/** \file root.h
 * \brief Root Sets.
 */
#ifndef tm_ROOT_H
#define tm_ROOT_H

int tm_root_add(const char *name, const void *l, const void *h);
void tm_root_remove(const char * name, const void *l, const void *h);

int tm_ptr_is_in_root_set(const void *ptr);

#endif
