/** \file list.h
 * \brief Colored, doubly-linked lists.
 *
 * $Id: list.h,v 1.8 2008-01-16 04:14:07 stephens Exp $
 */
#ifndef _tredmill_LIST_H
#define _tredmill_LIST_H

#include "tredmill/debug.h"

/****************************************************************************/
/*! \defgroup list List */
/*@{/*/

/**
 * A doubly-linked list header with 2 color bits hidden in the lower bits of the prev pointer.
 *
 * Assumes that tm_list objects are always word-aligned, thus the two lower bits are always zero.
 *
 * Can be used as a list on its own or a header for each list element. 
 */
typedef struct tm_list {
  /**
   * Pointer to next list element.
   * If used as a list header, pointer to first element in list.
   */
  struct tm_list *_next;
 
  /**
   * Pointer to prev list element with color encoded in lower 2 bits.
   */
  union {
    /*! Pointer to prev element, uncolored. */
    struct tm_list *_ptr;

    /*! Pointer word. */
    unsigned long _word;

    struct {
#ifndef TM_LIST__PREV__C
#if defined(__i386__) || defined(__i486__)
#define TM_LIST__PREV__C \
      unsigned long _color : 2; \
      unsigned long _bits  : sizeof(void*) * 8 - 2
#endif
#endif

#ifdef TM_LIST__PREV__C
      TM_LIST__PREV__C;
#else 
#error "Cannot determine layout of tm_list._prev._c; #define TM_LIST__PREV__C bit fields."
#endif
    } _c; /*!< bitfield struct containing color. */
  } _prev;
} tm_list;


/****************************************************************************/


/*! The color of a tm_list element. */
#define tm_list_color(l) (((tm_list*) (l))->_prev._c._color)

/*! Sets the color of a tm_list element. */
#define tm_list_set_color(l,c) (((tm_list*) (l))->_prev._c._color = (c))

/*! Sets the next pointer of a tm_list element. */
#define tm_list_set_next(l,x) (((tm_list*) (l))->_next = (x))

/*! Sets the prev pointer of a tm_list element. */
#define tm_list_set_prev(l,x) (((tm_list*) (l))->_prev._c._bits = ((unsigned long) (x) >> 2))

/*! The next element of a tm_list element. */
#define tm_list_next(l) ((void*) ((tm_list*) (l))->_next)

/*! The prev element of a tm_list element. */
#define tm_list_prev(l) ((void*) ((unsigned long) ((tm_list*) (l))->_prev._word & ~ 0x3UL))

/*! Initialize a static tm_list element as empty and color == 0. */
#define tm_list_INIT(N) tm_list _##N = { &_##N, { &_##N } }, *N = &_##N;

/**
 * Initialize a tm_list element as empty and color == 0;
 *
 * l->next = l->prev = l;
 */
static __inline 
void tm_list_init(void *l)
{
  ((tm_list *)l)->_next = l;
  ((tm_list *)l)->_prev._ptr = l;
}


/**
 * Returns true if a tm_list element is empty.
 */
static __inline 
int tm_list_empty(void *l)
{
  return tm_list_next(l) == l;
}


/**
 * Returns the first element of a tm_list element.
 * Or 0 if tm_list is empty.
 */
static __inline 
void * tm_list_first(void *l)
{
  return tm_list_empty(l) ? 
    0 : 
    tm_list_next(l);
}


/**
 * Returns the last element of a tm_list element.
 * Or 0 if tm_list is empty.
 */
static __inline 
void * tm_list_last(void *l)
{
  return tm_list_empty(l) ? 
    0 : 
    tm_list_prev(l);
}


/**
 * Removes a tm_list element from its list.
 * Element is marked empty.
 * Color of element is unchanged.
 */
static __inline 
void tm_list_remove(void *_p)
{
  tm_list *p = _p;

  tm_list_set_prev((tm_list*) tm_list_next(p), tm_list_prev(p));
  tm_list_set_next((tm_list*) tm_list_prev(p), tm_list_next(p));

  tm_list_set_next(_p, _p);
  tm_list_set_prev(_p, _p);
}


/**
 * Inserts tm_list element p at front of tm_list l.
 */
static __inline 
void tm_list_insert(void *l, void *p)
{
  tm_list_set_next(p, tm_list_next(l));
  tm_list_set_prev(p, l);

  tm_list_set_prev(tm_list_next(l), p);
  tm_list_set_next(l, p);
}


/**
 * Appends tm_list element p at end of tm_list l.
 */
static __inline 
void tm_list_append(void *l, void *p)
{
  tm_list_insert(tm_list_prev(l), p);
}


/**
 * Appends tm_list p at end of tm_list l.
 */
static __inline 
void tm_list_append_list(void *l, void *p)
{
  if ( ! tm_list_empty(p) ) {
    tm_list_set_prev(tm_list_next(p), tm_list_prev(l));
    tm_list_set_next(tm_list_prev(p), l);
    tm_list_set_next(tm_list_prev(l), tm_list_next(p));
    tm_list_set_prev(l, tm_list_prev(p));
  }

  tm_list_init(p);
}


/**
 * Removes tm_list element p and inserts into tm_list l.
 */
static __inline 
void tm_list_remove_and_insert(void *l, void *p)
{
  tm_list_remove(p);
  tm_list_insert(l, p);
}


/**
 * Removes tm_list element p and appends into tm_list l.
 */
static __inline 
void tm_list_remove_and_append(void *l, void *p)
{
  tm_list_remove(p);
  tm_list_append(l, p);
}


/**
 * Task first element from tm_list l.
 * Returns 0 if l is empty?
 */
static __inline 
void * tm_list_take_first(void *_l)
{
  tm_list *l = _l;
  if ( tm_list_next(l) != l ) {
    tm_list *p = tm_list_next(l);
    tm_list_remove(p);
    return p;
  } else {
    return 0;
  }
}


/*! Begin a loop on tm_list l, for each element x. */
#define tm_list_LOOP(l, x) do { tm_list *__l = (tm_list*) (l), *__x = tm_list_next(__l); while ( (void *) __x != (void *) (l) ) { x = (void*) __x; __x = tm_list_next(__x); {
/*! End a loop. */
#define tm_list_LOOP_END }}} while(0)


/**
 * Assert tm_list layout.
 */
static __inline
void tm_list_assert_layout()
{
  unsigned long word = 0x12345678UL;
  tm_list_INIT(l);
  tm_list_INIT(r);
  tm_list_INIT(x);
  
  x->_prev._ptr = (void*) word;
  tm_assert_equal(x->_prev._word, word, "%lu");
  tm_assert_equal(tm_list_prev(x), (void*) word, "%p");
  tm_assert_equal(tm_list_color(x), 0, "%d");
  tm_list_set_color(x, 3);
  tm_assert_equal(x->_prev._word,  word | 3, "%lu");
  tm_assert_equal(tm_list_prev(x), (void*) word, "%p");
  tm_assert_equal(tm_list_color(x), 3, "%d");

  tm_assert(tm_list_color(l) == 0);

  tm_assert_test(sizeof(tm_list*) == sizeof(void*));
  tm_assert_test(sizeof(tm_list*) == sizeof(unsigned long));

  tm_assert_test(tm_list_next(l) == l);
  tm_assert_test(tm_list_prev(l) == l);
  tm_assert_test(l->_prev._c._bits == ((unsigned long) (l)) >> 2);
  tm_assert_test(tm_list_color(l) == 0);
  
  tm_list_set_color(l, 3);
  tm_assert_test(tm_list_next(l) == l);
  tm_assert_test(tm_list_prev(l) == l);
  tm_assert_test(l->_prev._c._bits == ((unsigned long) (l)) >> 2);
  tm_assert_test(tm_list_color(l) == 3);

  tm_list_set_color(r, 2);
  tm_list_insert(l, r);
  tm_assert_test(tm_list_next(l) == (void*) r);
  tm_assert_test(tm_list_prev(l) == (void*) r);
  tm_assert_test(tm_list_next(r) == (void*) l);
  tm_assert_test(tm_list_prev(r) == (void*) l);
  tm_assert_test(tm_list_color(l) == 3);
  tm_assert_test(tm_list_color(r) == 2);

  tm_list_remove(r);
  tm_assert_test(tm_list_next(l) == (void*) l);
  tm_assert_test(tm_list_prev(l) == (void*) l);
  tm_assert_test(tm_list_next(r) == (void*) r);
  tm_assert_test(tm_list_prev(r) == (void*) r);
  tm_assert_test(tm_list_color(l) == 3);
  tm_assert_test(tm_list_color(r) == 2);

  tm_list_insert(r, l);
  tm_assert_test(tm_list_next(l) == (void*) r);
  tm_assert_test(tm_list_prev(l) == (void*) r);
  tm_assert_test(tm_list_next(r) == (void*) l);
  tm_assert_test(tm_list_prev(r) == (void*) l);
  tm_assert_test(tm_list_color(l) == 3);
  tm_assert_test(tm_list_color(r) == 2);
}


/*@}/*/


#endif
