#ifndef _tredmill_LIST_H
#define _tredmill_LIST_H

/* $Id: list.h,v 1.2 1999-06-09 23:39:59 stephensk Exp $ */

/****************************************************************************/

typedef struct tm_list {
  struct tm_list *next, *prev;
} tm_list;

/****************************************************************************/

#define tm_list_set_next(l,x) (((tm_list*) (l))->next = (x))
#define tm_list_set_prev(l,x) (((tm_list*) (l))->prev = (x))
#define tm_list_next(l) ((void*) ((tm_list*) (l))->next)
#define tm_list_prev(l) ((void*) ((tm_list*) (l))->prev)
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
  return l->next == l;
}

static __inline void * tm_list_first(void *_l)
{
  tm_list *l = _l;
  return tm_list_empty(_l) ? 0 : l->next;
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
  
  p->next = l->next;
  p->prev = l;

  l->next->prev = p;
  l->next = p;
}

static __inline void tm_list_append_list(void *_l, void *_p)
{
  tm_list *l = _l;
  tm_list *p = _p;

  if ( ! tm_list_empty(p) ) {
    p->next->prev = l->prev;
    p->prev->next = l;
    l->prev->next = p->next;
    l->prev = p->prev;
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
  if ( l->next != l ) {
    tm_list *p = l->next;
    tm_list_remove(p);
    return p;
  } else {
    return 0;
  }
}

#define tm_list_LOOP(l, x) do { tm_list *__l = (tm_list*) (l), *__x = __l->next; while ( __x != l ) { x = (void*) __x; __x = __x->next; {
#define tm_list_LOOP_END }}} while(0)


#endif
