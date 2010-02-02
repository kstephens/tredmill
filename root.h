/** \file root.h
 * \brief Root Sets.
 */
#ifndef tm_ROOT_H
#define tm_ROOT_H

/****************************************************************************/
/*! \defgroup root_set Root Set */
/*@{*/


/**
 * A root set region to be scanned for possible pointers.
 *
 * Pointers p within l >= p and p < h are considered within this root set.
 *
 * If callback is defined, callback(callback_data) is called.
 */
typedef struct tm_root {
  /*! The name of the root. */
  const char *name;

  /*! The low address of the root to be scanned. */
  const void *l;

  /*! The high address of the root to be scanned. */
  const void *h;

  /*! A callback. */
  void (*callback)(void *data);

  /*! Callback data */
  void *callback_data;
} tm_root;



int tm_root_add(const char *name, const void *l, const void *h);
void tm_root_remove(const char * name, const void *l, const void *h);

int tm_root_add_callback(const char *name, void (*callback)(void*), void *callback_data);
int tm_root_remove_callback(const char *name, void (*callback)(void*), void *callback_data);

int tm_ptr_is_in_root_set(const void *ptr);

/*@}*/

#endif
