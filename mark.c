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
void _tm_root_scan_all()
{
  int i;

  tm_msg("r G%lu B%lu {\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
  for ( i = 0; tm.roots[i].name; ++ i ) {
    _tm_root_scan_id(i);
  }
  tm.data_mutations = tm.stack_mutations = 0;
  _tm_root_loop_init();
#if 0
  tm_msg("r G%lu B%lu }\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
#endif
}


/**
 * Scan some roots.
 */
int _tm_root_scan_some()
{
  int result = 1;
  long left = tm_root_scan_some_size;

  tm_msg("r G%lu B%lu {\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
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
  tm_msg("r G%lu B%lu }\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
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


/**
 * Scans a node for internal pointers.
 *
 * If the node's type has user-defined allocation descriptor scan function,
 * call it.
 * Otherwise, scan the entire node's data for potential pointers.
 */
void _tm_node_scan(tm_node *n)
{
  tm_type *type = tm_node_type(n);

  if ( type->desc && type->desc->scan ) {
    type->desc->scan(type->desc, tm_node_ptr(n));
  } else {
    _tm_range_scan(tm_node_ptr(n), tm_node_ptr(n) + type->size);
  }

  /**
   * If the node was fully scanned,
   * Move the tm_GREY node to the marked (tm_BLACK) list.
   */
  tm_node_set_color(n, tm_node_to_block(n), tm_BLACK);

#if 0
  fprintf(stderr, "[B]");
  fflush(stderr);
#endif
}


/**
 * Scan node interiors for some pointers.
 *
 * Amount is in pointer-aligned words.
 */
size_t _tm_node_scan_some(size_t amount)
{
  size_t count = 0, bytes = 0;
#define gi (&tm.node_color_iter[tm_GREY])

  /*! Until amount has been scanned, or nothing is left to scan. */
  do {
    tm_node *n;

    /**
     * If a there is a tm_node region to scan,
     */
    if ( gi->scan_ptr ) {
      /*! Scan until end of tm_node region, */
      while ( gi->scan_ptr <= gi->scan_end ) {
	/*! Attempt to mark node at a possible pointer at the scan pointer, */
	_tm_mark_possible_ptr(* (void **) gi->scan_ptr);
      
	/*! And increment the current scan pointer. */
	gi->scan_ptr += tm_PTR_ALIGN;

	++ count;
	bytes += tm_PTR_ALIGN;

	/*! Stop scanning if the given amount has been scanned. */
	if ( -- amount <= 0 ) {
	  goto done;
	}
      }

      /**
       * If the node was fully scanned,
       * Move the tm_GREY node to the marked (tm_BLACK) list.
       */
      tm_node_set_color(gi->scan_node, tm_node_to_block(gi->scan_node), tm_BLACK);

#if 0
      fprintf(stderr, "B");
      fflush(stderr);
#endif

      /*! Reset scan pointers. */
      gi->scan_node = 0;
      gi->scan_ptr = 0;
      gi->scan_end = 0;
      gi->scan_size = 0;
    }
    /*! Done scanning gi->scan_node. */

    /*! If there is an tm_GREY node, */
    if ( (n = tm_node_iterator_next(gi)) ) {
      tm_assert_test(tm_node_color(n) == tm_GREY);

#if 0
      fprintf(stderr, "s");
      fflush(stderr);
#endif

      /**
       * Schedule the now BLACK node for interior pointer scanning.
       * Avoid words that overlap the end of the node's data region. 
       */
      gi->scan_node = n;
      gi->scan_ptr  = tm_node_ptr(n);
      gi->scan_size = gi->type->size;
      gi->scan_end  = gi->scan_ptr + gi->scan_size - sizeof(void*);
    } else {
      goto done;
    }
  } while ( amount > 0 );

 done:
#if 0
  if ( count )
    tm_msg("M c%lu b%lu l%lu\n", count, bytes, tm.n[tm_GREY]);
#endif

  /*! Return true if there are remaining tm_GREY nodes or some node is still being scanned. */
  return tm.n[tm_GREY] || gi->scan_size;
}

/**
 * Scan until all tm_GREY nodes are tm_BLACK.
 */
void _tm_node_scan_all()
{
  tm_node_LOOP_INIT(tm_GREY);
  while ( _tm_node_scan_some(~ 0UL) ) {
  }
}

#undef gi


/*@}*/
