#ifndef _tredmill_LIST_H
#define _tredmill_LIST_H

/* $Id: list.h,v 1.3 1999-06-16 08:35:07 stephensk Exp $ */

/****************************************************************************/

typedef struct tm_list {
  struct tm_list *_next;
  unsigned _prev : 30;
  unsigned _color : 2;
} tm_list;

/****************************************************************************/

#define tm_list_color(l) (((tm_list*) (l))->_color)
#define tm_list_set_color(l,c) (((tm_list*) (l))->_color = (c))

#define tm_list_set_next(l,x) (((tm_list*) (l))->_next = (x))
#define tm_list_set_prev(l,x) (((tm_list*) (l))->_prev = ((unsigned long)(x) >> 2))
#define tm_list_next(l) ((void*) ((tm_list*) (l))->_next)
#define tm_list_prev(l) ((void*) (((tm_list*) (l))->_prev << 2))
#define tm_list_INIT(N) tm_list _##N = { &_##N, &_##N }, *N = &_##N;

static __inline void tm_list_init(void *_l)
{
  tm_list *l = _l;
  tm_list_set_next(l, l);
  tm_list_set_prev(l, l);
}

static __inline int tm_list_empty(void *_l)
{
  tm_list *l = _l;
  return tm_list_next(l) == l;
}

static __inline void * tm_list_first(void *_l)
{
  tm_list *l = _l;
  return tm_list_empty(_l) ? 0 : tm_list_next(l);
}

static __inline void tm_list_remove(void *_p)
{
  tm_list *p = _p;

  tm_list_set_prev((tm_list*) tm_list_next(p), tm_list_prev(p));
  tm_list_set_next((tm_list*) tm_list_prev(p), tm_list_next(p));

  tm_list_init(p);
}

static __inline void tm_list_insert(void *_l, void *_p)
{
  tm_list *l = _l;
  tm_list *p = _p;
  
  tm_list_set_next(p, tm_list_next(l));
  tm_list_set_prev(p, l);

  tm_list_set_prev(tm_list_next(l), p);
  tm_list_set_next(l, p);
}

static __inline void tm_list_append_list(void *_l, void *_p)
{
  tm_list *l = _l;
  tm_list *p = _p;

  if ( ! tm_list_empty(p) ) {
    tm_list_set_prev(tm_list_next(p), tm_list_prev(l));
    tm_list_set_next(tm_list_prev(p), l);
    tm_list_set_next(tm_list_prev(l), tm_list_next(p));
    tm_list_set_prev(l, tm_list_prev(p));
  }

  tm_list_init(p);
}

static __inline void tm_list_remove_and_insert(void *_l, void *_p)
{
  tm_list_remove(_p);
  tm_list_insert(_l, _p);
}

static __inline void * tm_list_take_first(void *_l)
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

#define tm_list_LOOP(l, x) do { tm_list *__l = (tm_list*) (l), *__x = tm_list_next(__l); while ( __x != l ) { x = (void*) __x; __x = tm_list_next(__x); {
#define tm_list_LOOP_END }}} while(0)


#endif
