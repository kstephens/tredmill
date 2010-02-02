/** \file mark.c
 * \brief Marking primitives.
 */
#include "internal.h"


/***************************************************************************/
/*! \defgroup root_set_scan Root Set: Scanning */
/*@}*/

/*
 * Scan an address range for potential pointers.
 */
__inline
void _tm_range_scan(const void *b, const void *e)
{
  const char *p;

  /* Avoid overlapping pointer. */
  e = ((char *) e) - sizeof(void*);

  for ( p = b; 
	(char*) p <= (char*) e; 
	p += tm_PTR_ALIGN ) {
#if 0
    tm_node *n = 
#endif
      _tm_mark_possible_ptr(* (void**) p);

#if 0
    if ( n ) {
      tm_msg("M n%p p%p\n", n, p);
    }
#endif
  }
}


/*! Amount of roots words to scan per tm_malloc() */
long tm_root_scan_some_size = 512;

/**
 * Initialize root scanning loop.
 */
void _tm_root_loop_init()
{
  tm.rooti = tm.root_datai;
  tm.rp = tm.roots[tm.rooti].l;
  tm.data_mutations = tm.stack_mutations = 0;
}


/**
 * Scan a root id.
 */
static
void _tm_root_scan_id(int i)
{
  if ( tm.roots[i].l < tm.roots[i].h ) {
    tm_msg("r [%p,%p] %s\n", tm.roots[i].l, tm.roots[i].h, tm.roots[i].name);
    _tm_range_scan(tm.roots[i].l, tm.roots[i].h);
  }
  if ( tm.roots[i].callback ) {
    tm_msg("r %p(%p) %s\n", tm.roots[i].callback, tm.roots[i].callback_data, tm.roots[i].name);
    tm.roots[i].callback(tm.roots[i].callback_data);
  }
}

/**
 * Scan registers.
 * 
 * Registers are in tm.root[0].
 */
void _tm_register_scan()
{
  _tm_root_scan_id(0);
}

/**
 * Set the stack pointer.
 *
 * Adjust for slack.
 */
void _tm_set_stack_ptr(void *stackvar)
{
  *tm.stack_ptrp = (char*) stackvar - 64;
}


/**
 * Scan stack (and registers).
 *
 * Mark stack as un-mutated.
 */
void _tm_stack_scan()
{
  _tm_register_scan();
  _tm_root_scan_id(1);
  tm.stack_mutations = 0;
}


/**
 * Scan all roots.
 */
void tm_root_scan_all()
{
  int i;

  tm_msg("r G%lu B%lu {\n", tm.n[GREY], tm.n[BLACK]);
  for ( i = 0; tm.roots[i].name; ++ i ) {
    _tm_root_scan_id(i);
  }
  tm.data_mutations = tm.stack_mutations = 0;
  _tm_root_loop_init();
#if 0
  tm_msg("r G%lu B%lu }\n", tm.n[GREY], tm.n[BLACK]);
#endif
}


/**
 * Scan some roots.
 */
int _tm_root_scan_some()
{
  int result = 1;
  long left = tm_root_scan_some_size;

  tm_msg("r G%lu B%lu {\n", tm.n[GREY], tm.n[BLACK]);
  tm_msg("r [%p,%p] %p(%p) %s\n", 
	 tm.roots[tm.rooti].l, 
	 tm.roots[tm.rooti].h,
	 tm.roots[tm.rooti].callback, 
	 tm.roots[tm.rooti].callback_data, 
	 tm.roots[tm.rooti].name 
	 );

  do {
    /* Try marking some roots. */
    while ( (void*) (tm.rp + sizeof(void*)) >= tm.roots[tm.rooti].h ) {
      ++ tm.rooti;
      if ( ! tm.roots[tm.rooti].name ) {
	_tm_root_loop_init();
	tm_msg("r done\n");

	result = 0;
	goto done;
      }

      tm_msg("r [%p,%p] %p(%p) %s\n", 
	     tm.roots[tm.rooti].l, 
	     tm.roots[tm.rooti].h,
	     tm.roots[tm.rooti].callback,
	     tm.roots[tm.rooti].callback_data,
	     tm.roots[tm.rooti].name);
      
      tm.rp = tm.roots[tm.rooti].l;
    }

    _tm_mark_possible_ptr(* (void**) tm.rp);

    tm.rp += tm_PTR_ALIGN;
    left -= tm_PTR_ALIGN;

  } while ( left > 0 );

 done:
#if 0
  tm_msg("r G%lu B%lu }\n", tm.n[GREY], tm.n[BLACK]);
#endif

  return result; /* We're not done. */
}

/*@}*/

/***************************************************************************/
/*! \defgroup marking Marking */
/*@{*/

/**
 * Marks a possible pointer.
 *
 * tm_root_write() should be called after modifing a root ptr.
 */
void tm_mark(void *ptr)
{
  _tm_mark_possible_ptr(ptr);
}


/*@}*/
