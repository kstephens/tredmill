/** \file mark.c
 * \brief Marking primitives.
 */
#include "internal.h"


/***************************************************************************/
/*! \defgroup root_set_scan Root Set: Scanning */
/*@}*/

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
  tm_msg("r [%p,%p] %s\n", 
	 tm.roots[tm.rooti].l, 
	 tm.roots[tm.rooti].h,
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

      tm_msg("r [%p,%p] %s\n", 
	     tm.roots[tm.rooti].l, 
	     tm.roots[tm.rooti].h,
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

/*
 * Scan an address range for potential pointers.
 */
void _tm_range_scan(const void *b, const void *e)
{
  const char *p;

  for ( p = b; 
	((char*) p + sizeof(void*)) <= (char*) e; 
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


#if 0
static __inline
void _tm_node_mark_and_scan_interior(tm_node *n, tm_block *b)
{
  tm_assert_test(tm_node_to_block(n) == b);
  tm_assert_test(tm_node_color(n) == tm_GREY);

  /* Mark all possible pointers. */
  {
    const char *ptr = tm_node_to_ptr(n);
    _tm_range_scan(ptr, ptr + b->type->size);
  }
}
#endif


/**
 * Scan node interiors for some pointers.
 */
size_t _tm_node_scan_some(size_t amount)
{
  size_t count = 0, bytes = 0;
#define gi (&tm.node_color_iter[tm_GREY])

  /*! Until amount has been scanned, or nothing is left to scan. */
  do {
    tm_node *n;

    /*! Skip non-BLACK nodes. */
    if ( gi->node && ! tm_node_color(gi->node) == tm_BLACK ) {
      gi->ptr = 0;
    }

    /**
     * Check node color because write barrier might
     * have move the current scan node to GREY.
     */
    if ( gi->ptr ) {
      while ( gi->ptr + sizeof(void*) < gi->end ) {
	/* Attempt to mark node at possible pointer. */
	_tm_mark_possible_ptr(* (void **) gi->ptr);
      
	gi->ptr += tm_PTR_ALIGN;

	++ count;
	bytes += tm_PTR_ALIGN;

	/* Scanned enough? */
	if ( -- amount <= 0 ) {
	  goto done;
	}
      }

      /**
       * Done scanning node:
       * Reset scan pointer because all scanning maybe complete.
       */
      gi->node = 0;
      gi->ptr = 0;
      gi->end = 0;
      gi->size = 0;
    }
    /*! Done scanning gi->node. */

    /*! If next GREY node, */
    if ( (n = tm_node_iterator_next(gi)) ) {
      tm_assert_test(tm_node_color(n) == tm_GREY);

      /*! Move to marked (BLACK) list. */

      /**
       * However set the color of the node to BLACK first, 
       * before scanning for interior pointers.
       *
       * This ensures if
       * this node is mutated during
       * interior pointer scanning,
       * the node will be put back on
       * the tm_GREY list by the write barrier.
       */
      tm_node_set_color(n, tm_node_to_block(n), tm_BLACK);

      /*! Schedule this node for interior pointer scanning. */
      gi->ptr  = tm_node_ptr(n);
      gi->size = gi->type->size;
      gi->end  = gi->ptr + gi->size;
    } else {
      goto done;
    }
  } while ( amount > 0 );

 done:
#if 0
  if ( count )
    tm_msg("M c%lu b%lu l%lu\n", count, bytes, tm.n[tm_GREY]);
#endif

  /*! Return true if there are remaining tm_GREY nodes or some pending node left. */
  return tm.n[tm_GREY] || gi->size;
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
