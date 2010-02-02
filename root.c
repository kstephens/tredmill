/** \file root.c
 * \brief Root Sets
 */
#include "internal.h"

/****************************************************************************/

/**
 * Add a single root set.
 */
static 
int _tm_root_add_1(tm_root *a)
{
  int i;
#define MAX_ROOTS (sizeof(tm.roots)/sizeof(tm.roots[0]))

  if ( a->l >= a->h )
    return -1;

  /* Scan for empty slot. */
  for ( i = 0; i < MAX_ROOTS; ++ i ) {
    if ( tm.roots[i].name == 0 || tm.roots[i].l == tm.roots[i].h ) {
      break;
    }
  }

  if ( tm.nroots <= i ) {
    tm.nroots = i + 1;
  }

  tm_assert_test(i < MAX_ROOTS);
  tm_assert_test(i >= tm.root_datai);

  if ( tm.root_newi == -1 ) {
    tm.root_newi = i;
  }

  tm.roots[i] = *a;

  tm_msg("R a [%p,%p] %p(%p) %s %d\n", 
	 tm.roots[i].l, 
	 tm.roots[i].h,
	 tm.roots[i].callback,
	 tm.roots[i].callback_data,
	 tm.roots[i].name,
	 i);

  return i;
}
#undef MAX_ROOTS


/**
 * Subtract b from a, returning result in c.
 *
 */
static 
int tm_root_subtract(tm_root *a, tm_root *b, tm_root *c)
{
  const void *tmp;

  if ( a->l > a->h ) {
    tmp = a->l; a->l = a->h; a->h = tmp;
  }
  if ( b->l > b->h ) {
    tmp = b->l; b->l = b->h; b->h = tmp;
  }
  /*! Return 0 if b is empty. */
  if ( b->l == b->h ) {
    return 0;
  }

  if ( (a->l == b->l) || (b->l <= a->l && a->h <= b->h) ) {
    /**
     * Case: b in a:
     *
     * <pre>
     *
     * a   L-------------------------------------------H
     * b      L--------------------H
     * c   0
     *
     * </pre>
     *
     * Return -1.
     */
    return -1; /* Delete flag */
  }
  if ( b->h <= a->l || b->l >= a->h ) {
    /**
     * Case: b out of a:
     *
     * <pre>
     *
     * a   L-------------H                        L--------------H
     * b                   L--------------------H
     * c   0
     *
     * </pre>
     *
     * Return 0.
     */
    /* *c = *a; */
    return 0;
  }
  if ( a->l < b->l && b->h < a->h ) {
    /**
     * Case: b is in a:
     *
     * <pre>
     *
     * a            L------H
     * b   H-----------------------H
     * c1                  L-------H
     * c2  L--------H
     *
     * </pre>
     *
     * Save c1 and c2.
     * Return 2.
     */
    c[0] = *a;
    c[0].l = b->h;
    c[1] = *a;
    c[1].h = b->l;
    return 2;
  }
  if ( a->l < b->h && b->h <= a->h ) {
    /**
     * Case: b.h in a:
     *
     * <pre>
     *
     * a          L---------------------H
     * b    L--------------------H
     * c                         L------H
     *
     * </pre>
     *
     * Return 1.
     */
    *c = *a;
    c->l = b->h;
    return 1;
  }
  if ( a->l < b->l && b->l <= a->h ) {
    /**
     * Case: b.l in a:
     *
     * <pre>
     *
     * a   L---------------------H
     * b             L--------------------H
     * c   L---------H
     *
     * </pre>
     *
     * Return 1.
     */
    *c = *a;
    c->h = b->l;
    return 1;
  }

  tm_abort();
  return -1;
}


/**
 * Add a root set, while splitting it by anti-roots.
 */
static 
void _tm_root_add(tm_root *a)
{
  int i;
  tm_root c[2];

  /* Scan anti-roots for possible splitting */
  for ( i = 0; i < tm.naroots; ++ i ) {
    switch ( tm_root_subtract(a, &tm.aroots[i], c) ) {
    case -1: /* deleted */
      return;
      break;
      
    case 0: /* OK */
      break;
      
    case 1: /* clipped */
      *a = c[0];
      break;
      
    case 2: /* split */
      _tm_root_add(&c[0]);
      *a = c[1];
      break;
    }
  }

  _tm_root_add_1(a);
}

/**
 * API: Add a root set.
 */
int tm_root_add(const char *name, const void *l, const void *h)
{
  tm_root a;

  a.name = name;
  a.l = l;
  a.h = h;
  a.callback = 0;

  tm.root_newi = -1;

  tm_msg("R A [%p,%p] %s PRE\n", 
	 a.l, 
	 a.h,
	 a.name);

  _tm_root_add(&a);

  return tm.root_newi;
}


/**
 * API: Add a root set.
 */
int tm_root_add_callback(const char *name, void (*callback)(void*), void *callback_data)
{
  tm_root a;

  a.name = name;
  a.l = a.h = 0;
  a.callback = callback;
  a.callback_data = callback_data;

  tm.root_newi = -1;

  tm_msg("R A %p(%p) %s PRE\n", 
	 a.callback,
	 a.callback_data,
	 a.name);

  _tm_root_add(&a);

  return tm.root_newi;
}


/**
 * API: Add a root set.
 *
 * Returns true if a root was removed.
 */
int tm_root_remove_callback(const char *name, void (*callback)(void*), void *callback_data)
{
  int i;
  for ( i = 0; i < tm.root_newi; ++ i ) {
    tm_root *r = &tm.roots[i];
    if ( (name && strcmp(name, r->name) == 0) ||
	 (callback && callback == r->callback && callback_data == r->callback_data) ) {
      -- tm.root_newi;
      while ( i < tm.root_newi ) {
	r[0] = r[1];
	++ r;
	++ i;
      }
      return 1;
      break;
    }
  }

  return 0;
}


void tm_root_remove(const char * name, const void *l, const void *h)
{
  int i;
  tm_root *a, *b, c[2];

  /* Add to anti-roots list. */
  b = &tm.aroots[i = tm.naroots ++];
  b->name = name;
  b->l = l;
  b->h = h;

  tm_msg("R A [%p,%p] %s ANTI-ROOT %d\n", 
	 tm.aroots[i].l, 
	 tm.aroots[i].h,
	 tm.aroots[i].name,
	 i);

  /* Adding a anti-root might require splitting existing roots. */
  /* Do not split tm.root[0], it's the machine register set! */

  for ( i = tm.root_datai; i < tm.nroots; ++ i ) {
    int j;
     
    for ( j = 0; j < tm.naroots; ++ j ) {
      a = &tm.roots[i];
      b = &tm.aroots[j];
      
      switch ( tm_root_subtract(a, b, c) ) {
      case -1: /* deleted */
	a->l = a->h = 0;
	j = tm.naroots;
	break;
	
      case 1: /* clipped */
	*a = *c;
	break;
	
      case 2: /* split */
	*a = *c;
	_tm_root_add(&c[1]);
	i = 1; j = -1; /* restart */
	break;
      }
    }
  }
}


int tm_ptr_is_in_root_set(const void *ptr)
{
  int i;

  for ( i = 0; i < tm.nroots; ++ i ) {
    if ( tm.roots[i].l <= ptr && ptr < tm.roots[i].h ) {
      return i + 1;
    }
  }

  return 0;
}


