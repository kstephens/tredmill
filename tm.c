/** \file tm.c
 * \brief Internals
 *
 * - $Id: tm.c,v 1.20 2009-08-01 10:47:31 stephens Exp $
 */

/** \mainpage TM: a real-time conservative allocator.
\section Introduction

TM is a real-time, non-relocating, conservative, allocator using type- and color-segregated lists, 
inspired by Henry Baker’s "Treadmill" paper. 

The source code for TM is located at http://kurtstephens.com/pub/tredmill.

TM interleaves marking, scanning and sweeping during each call to tm_alloc().

TM attempts to limit the amount of scanning, marking and sweeping
during each call to tm_alloc() to avoid "stopping the world" for long
periods of time.

TM supports type segregation to increase locality of objects of the same type and
to reduce internal fragmentation.

The space overhead of each allocated object requires an object header (tm_node) of two address pointers.

\section more_info More Information

See:
- http://kurtstephens.com/research/tredmill
- The Treadmill: Real-Time Garbage Collection Without Motion Sickness, Henry G. Baker, http://home.pipeline.com/~hbaker1/NoMotionGC.html
.

\section to_do To Do

- Support for 64-bit address spaces.
- Implement allocations larger than tm_block_SIZE.
- Implement aligned allocations using a page-indexed bit vector.
  - Flag a bit for the beginning page of the aligned block.
  - Use a hash table to map the address of the aligned block to a tm_block containing its own tm_type.
  .
.

\section implementation Implementation

\subsection data_structures Data Structures

An allocation unit is a tm_node. 
Each tm_node is allocated from a block of system memory (tm_block) sized and aligned to a multiple of the operating system’s page size.
Each tm_block is dedicated to a particular type (tm_type). 
Each tm_type represents a tm_node allocation size, thus each tm_block is uniformly parceled into tm_nodes of the same size.
In general, each power-of-two size has a tm_type.
However, tm_types can be explicitly created for any size.

\subsection node_coloring Node Coloring

Each tm_node has a “color” tag describing its current allocation state:

-# tm_WHITE: free, unused.
-# tm_ECRU: allocated, potentially unused.
-# tm_GREY: allocated, marked used, not scanned for pointers.
-# tm_BLACK: allocated, marked used, scanned for pointers.

Each tm_type has it's own tm_WHITE, tm_ECRU, tm_GREY, and tm_BLACK doubly-linked lists.
Each tm_node is a member of one, and only one, of its type’s color lists. 
To keep the tm_node header small, the node's color is encoded in the lower two bits of the tm_node._prev pointer.

When a tm_node's color changes, it is moved to a different colored list in its tm_type for processing in a different allocation phase.

Node types can be explicitly created for common sizes that are not a power of two, to reduce internal memory fragmentation.

\subsection allocation_phases Allocation Phases

The allocator interleaves the following phases with allocation:

-# tm_ALLOC: Allocate from OS or parcel nodes from allocated blocks. (tm_WHITE -> tm_ECRU)
-# tm_UNMARK: Unmarking scanned nodes for subsequent rooting. (tm_BLACK -> tm_ECRU)
-# tm_ROOT: Root scanning and marking nodes for scanning. (tm_ECRU -> tm_GREY)
-# tm_SCAN: Scanning marked nodes for interior pointers. (tm_GREY -> tm_BLACK)
-# tm_SWEEP: Freeing unmarked nodes. (tm_ECRU -> tm_WHITE)

During each call to tm_alloc(), each phase will do some collection work before returning a newly allocated tm_node:

-# tm_ALLOC: Parcel nodes from allocated blocks.  Memory pressure may cause switching to the tm_UNMARK phase.
-# tm_UNMARK: Allocate tm_nodes from the tm_type’s tm_WHITE list until it is empty. A fixed number of tm_BLACK nodes are returned to the tm_ECRU list. Memory pressure may cause switching to the tm_ROOT phase.
-# tm_ROOT: A fixed number of roots are scanned for pointers into allocated space. tm_ECRU nodes with valid references are moved to tm_GREY. After all roots are scanned, the next phase is tm_SCAN.
-# tm_SCAN: A fixed number of node bytes of tm_GREY nodes are scanned for interior pointers and moved to tm_BLACK list. Once all tm_GREY nodes are scanned, the next phase is tm_SWEEP.
-# tm_SWEEP: If there are no more nodes to be scanned, sweep a fixed number of tm_ECRU nodes to the tm_WHITE list. Once all tm_ECRU nodes are sweeped, the collector returns to the tm_UNMARK phase.

\subsection node_states Node States

As nodes are parceled, allocated, marked, scanned, sweeped and unmarked they are moved between the colored lists as follows:

\image html tredmil_states.png "Node States and Transitions"
\image latex tredmil_states.png "Node States and Transitions" width=10cm

Each tm_type maintains a separate list for each tm_node color.  
Statistics of the colors of all tm_nodes are kept at the tm_type, tm_block and global level in arrays indexed by color.  
A total count of all nodes at each level is kept at the offset of tm_N.

In general, newly allocated tm_nodes are taken from the tm_type’s tm_WHITE list and placed on its tm_ECRU list.
If the type’s tm_WHITE list is empty, a tm_block is allocated from the operating system and is scheduled for parceling new tm_WHITE nodes for the type. 
If the allocation tm_block becomes empty, the process is repeated: another tm_block is allocated and parceled.

A limited number of new free nodes are parceled from the type’s current allocation block as needed and are initialized as new tm_WHITE nodes.

New tm_blocks may be requested from the operating system during all phases, if the type’s tm_WHITE list or allocation block is empty.
The reasoning is the operating system should be able to allocate a new allocation block faster than a collection that would need to completely “stop the world”.

All phases, except the tm_SWEEP phase, allocate nodes by moving them from the tm_WHITE list to the tm_ECRU list.
The tm_SWEEP phase allocates nodes from tm_WHITE to tm_GREY, because tm_ECRU nodes are considered to be 
unmarked garbage during the tm_SWEEP phase and tm_BLACK nodes are considered “in-use’ -- 
tm_GREY nodes are not sweeped during the tm_SWEEP phase.
Using tm_GREY during tm_SWEEP is also related to tm_BLACK node mutation, 
which gives rise to requiring a write barrier on tm_BLACK nodes.

The tm_SWEEP phase will always attempt to scan any tm_GREY nodes before continuing to sweep any tm_ECRU nodes.

The tm_BLACK color might seem a better choice for tm_SWEEP allocations, 
but this would force young nodes, which are more likely to be garbage, to be kept until the next tm_SWEEP phase. 
Coloring tm_SWEEP allocations tm_BLACK would also prevent any new interior pointers stored in nodes that may reference tm_ECRU nodes from being scanned before tm_SWEEP is complete.

If once tm_SWEEP is complete, tm_blocks with no active tm_nodes (i.e. b->n[tm_WHITE] == b->n[tm_TOTAL]) have all their tm_nodes unparcelled and the tm_block is returned to the operating system.

The tm_SCAN phase may cause a full GC if its determined that the last remaining node to scan is being mutated.
The tm_SCAN phase may also cause all tm_GREY nodes to be scanned during tm_malloc(), if memory pressure is high.
The tm_SCAN phase always scans all roots if there are no tm_GREY nodes left, before continuing to the tm_SWEEP phase.

To prevent thrashing the operating system with tm_block allocation and free requests, 
a limited number of unused blocks are kept on a global tm_block free list.

\subsection aligned_blocks Aligned Blocks

Aligned blocks allow determining if generic word is a pointer to a heap-allocated tm_node to be computed with simple pointer arithmetic and a bit vector check.

To determine a pointer’s allocation:

-# Checking a bit vector indexed by the pointer’s page number. The bit is set when tm_nodes are parceled and cleared when the entire tm_block is unused and returned to the free block list or operating system.
-# Mask off the insignificant page bits to construct an aligned tm_block address.
-# Determine the size of tm_nodes in the tm_block from the block’s tm_type.
-# Determine if the pointer resides in the data portion of the tm_node by considering the tm_node and tm_block linked-list headers to be “holes” in the address space.

\subsection type_segregation Type Segregation

Nodes are segregated by type. Each type has a size. By default, type sizes are rounded up to the next power of two. A specific allocation type can be requested with tm_adesc_for_size(). The allocation descriptor can then be used by tm_alloc_desc(). The opaque element can be used to store additional mutator data.

Each node type has its own colored lists, allocated block lists and accounting. Segregating node types allows allocation requests to be done without scanning tm_WHITE nodes for best fit. However, since types and the blocks are segregated, and nodes of a larger size are not scavenged for smaller sise, this could least to poor actual memory utilization in mutators with small numbers of allocations for many sizes, since a single node allocation for a given size will cause at least one block to requested from the operating system.

The color lists are logically combined from all types for iteration using nested type and node iterators.

\subsection data_structures Data Structures

- There is a single, statically allocated tm_data object.
- A tm_list is used as a doubly-linked list header.  It also encodes color in the lowest bits of its prev pointer.
- The tm_data object has a tm_list of tm_type objects. 
- Each tm_type has a tm_list of tm_blocks.
- Each tm_block is associated with a tm_type.
- Each tm_type has 4 color tm_list objects containing tm_node objects of the same color.
- Each tm_block has been parceled into tm_node objects sized by its tm_type.
- Each tm_data, tm_type and tm_block object has counters for each tm_node color.

\image html  data_structure.png "Example tm_data structure"
\image latex data_structure.png "Example tm_data structure" width=10cm

The above example has a single tm_type of size 16 with two tm_block objects, and 8 tm_node objects parceled the tm_block objects.
White tm_node objects are rendered here with a dotted style.

\subsection write_barriers Write Barriers

During the tm_SCAN and tm_SWEEP phase, any tm_BLACK node that is mutated must be rescanned due to the possible introduction of new references from the tm_BLACK node to tm_ECRU nodes. This is achieved by calling tm_write_barrier(&R) in the mutator after modifying R’s contents.

There are three versions of the write barrier:

-# tm_write_barrier(R)
-# tm_write_barrier_pure(R)
-# tm_write_barrier_root(R)

tm_write_barrier_pure(R) can be called when R is guaranteed to be a pointer to the head of a node allocated by tm_alloc(). 
tm_write_barrier_pure(R) cannot be used if R might be an interior pointer or a pointer to a stack or root-allocated object. 
tm_write_barrier(R) should be used if the address of R is not known to be a pointer to the heap, the stack or global roots.
tm_write_barrier_root(R) should be used if the address of R is on the stack or global roots.
If R is a pointing into global roots, tm_write_barrier(R) will cause global root rescanning, if the collector is in the tm_SCAN phase.

Stack writes are not barriered, because stack scanning occurs atomically at the end of tm_ROOT.

\subsection unfriendly_mutators Unfriendly Mutators

When entering code where the write barrier protocol is not followed, tm_disable_write_barrier() can be called to put the collector into a “stop-world” collection mode until tm_enable_write_barrier() is called.

A virtual memory write-barrier based on mprotect() might be easier to manage than requiring the mutator to call the write barrier. 
Recoloring and marking root set pages can be done in hardware assuming the overhead of mprotect() and the SIGSEGV signal handler is low when changing phases and colors.

\subsection issues Issues

- Due to the altering of tm_node headers during all allocation phases, forked processes will mutate pages quickly.
- TM is not currently thread-safe.
- TM is not currently operational on 64-bit archtectures due to statically allocation block bit maps.
- TM does not currently support allocations larger than a tm_block. This will be fixed by using another page-indexed bit vector. A “block-header-in-page” bit vector marks the page of each tm_block header. This bit vector will be scanned backwards to locate the first page that contains the allocation’s block header.
- TM does not currently support requests for page-aligned allocations. This could be achieved by using a hash table to map page-aligned allocations to its tm_block.
- TM does not "switch" the rolls of ECRU and BLACK after marking as list in Baker's paper.

\subsection references References

-# The Treadmill: Real-Time Garbage Collection Without Motion Sickness, Henry G. Baker, http://home.pipeline.com/~hbaker1/NoMotionGC.html

 
 */

#if 0

#include "tm.h"
#include "internal.h"


/**************************************************************************/
/*! \defgroup color Color */
/*@{*/

/*
This table defines what color a newly allocated node
should be during the current allocation phase.
*/

/**
 * Default allocated nodes to "not marked".
 *
 * Rational: some are allocated but thrown away early.
 */
#define tm_DEFAULT_ALLOC_COLOR ECRU

/**
 * Default allocated node color during sweep.
 *
 * Rational: 
 * tm_ECRU is considered garabage during tm_SWEEP.
 *
 * Assume that new nodes are actually in-use and may contain pointers 
 * to prevent accidental sweeping.
 */
#define tm_SWEEP_ALLOC_COLOR GREY

/*@}*/

/****************************************************************************/
/*! \defgroup validation Validation */
/*@{*/

void tm_validate_lists();


/*@}*/


/**************************************************************************/
/*! \defgroup internal_data Internal: Data */
/*@{*/

/**
 * Global allocator data.
 */
struct tm_data tm;


/*@}*/

/***************************************************************************/
/* \defgroup color Color */
/*@{*/


/**
 * Set a tm_node color within a tm_block.
 *
 * The tm_node must reside in the tm_block.
 *
 * Removes the tm_node from its current tm_type color list
 * and appends the tm_node to the new color_list.
 */
void tm_node_set_color(tm_node *n, tm_block *b, tm_color c)
{
  tm_type *t;

  tm_assert_test(b);
  // _tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;

  _tm_node_set_color(n, b, t, c);

  tm_list_remove_and_append(&t->color_list[c], n);
 
  // _tm_validate_lists();
}


/*@}*/

/*********************************************************************/
/*! \defgroup node Node */
/*@{*/



/*@}*/

/***************************************************************************/
/*! \def group sweep Sweep */
/*@{*/


/**
 * Sweep an ECRU node to the free list.
 * If its tm_block is unused, free the block also.
 */
static __inline 
void _tm_node_sweep(tm_node *n, tm_block *b)
{
  /*! Make sure the node is unmarked and not in use. */
  tm_assert_test(tm_node_color(n) == ECRU);
  // _tm_block_validate(b);

  /*! Clear out the data portion of the tm_node. */
  memset(tm_node_to_ptr(n), 0, b->type->size);

  /*! Put the tm_node on its tm_type free (tm_WHITE) list. */
  tm_node_set_color(n, b, WHITE);

#if 0
  tm_msg("s n%p\n", n);
#endif

  /*! Free the tm_node's tm_block if the tm_block completely unused. */
  if ( tm_block_unused(b) ) {
    _tm_block_free(b);
  }
}


/**
 * Sweep some ECRU nodes of any type.
 */
static 
size_t _tm_node_sweep_some(int left)
{
  size_t count = 0, bytes = 0;
 
  if ( tm.n[ECRU] ) {
    if ( _tm_check_sweep_error() )
      return 0;

    tm_node_LOOP(ECRU);
    {
      tm_assert_test(tm_node_color(n) == ECRU);

      _tm_node_sweep(n, tm_node_to_block(n));

      bytes += t->size;
      ++ count;

      if ( -- left <= 0 ) {
	tm_node_LOOP_BREAK(ECRU);
      }
    }
    tm_node_LOOP_END(ECRU);
  }

  /*! Decrement global stats. */
  tm.n[tm_b] -= bytes;

#if 0
  if ( count ) 
    tm_msg("s c%lu b%lu l%lu\n", count, bytes, tm.n[ECRU]);
#endif

  /*! Return the global number of ECRU nodes. */
  return tm.n[ECRU];
}


/**
 * Sweep some nodes for a tm_type.
 */
static 
size_t _tm_node_sweep_some_for_type(tm_type *t, int left)
{
  size_t count = 0, bytes = 0;
  tm_node *n;

  while ( t->n[ECRU] ) {
    tm_assert(tm_list_first(&t->color_list[ECRU]));

    n = tm_list_first(&t->color_list[ECRU]);
    fprintf(stderr, "n = %p\n", (void*) n);

    _tm_node_sweep(n, tm_node_to_block(n));
    
    bytes += t->size;
    ++ count;
    
    if ( -- left <= 0 ) {
      break;
    }
  }

  /*! Decrement global allocation stats. */
  tm.n[tm_b] -= bytes;

#if 0
  if ( count ) 
    tm_msg("s t%p c%lu b%lu l%lu\n", t, count, bytes, tm.n[ECRU]);
#endif

  /*! Return the number of ECRU nodes for the tm_type. */
  return t->n[ECRU];
}


/**
 * Sweep all ECRU nodes back to free lists.
 */
static 
void _tm_node_sweep_all()
{
  /*! While there are tm_ECRU nodes, */
  while ( tm.n[ECRU] ) {
    /*! Keep sweeping them. */
    tm_node_LOOP_INIT(ECRU);
    _tm_node_sweep_some(tm.n[ECRU]);
  }
}


/*@}*/

/***************************************************************************/
/*! \defgroup unmark Unmark */
/*@{*/


/**
 * Unmark some BLACK nodes.
 */
static 
size_t _tm_node_unmark_some(long left)
{
  size_t count = 0, bytes = 0;

  tm_node_LOOP(BLACK);
  {
    tm_assert_test(tm_node_color(n) == BLACK);
    
    tm_node_set_color(n, tm_node_to_block(n), ECRU);
    
    bytes += t->size;
    ++ count;
    
    if ( -- left <= 0 ) {
      tm_node_LOOP_BREAK(BLACK);
    }
  }
  tm_node_LOOP_END(BLACK);

#if 0
  if ( count ) 
    tm_msg("u n%lu b%lu l%lu\n", count, bytes, tm.n[BLACK]);
#endif

  return tm.n[BLACK];
}


/**
 * Unmark all BLACK nodes.
 */
static 
void _tm_node_unmark_all()
{
  while ( tm.n[BLACK] ) {
    tm_node_LOOP_INIT(BLACK);
    _tm_node_unmark_some(tm.n[tm_TOTAL]);
  }
}


/*@}*/

/***************************************************************************/
/*! \defgroup gc GC */
/*@{*/


/**
 * Force a full GC for a specific type.
 *
 * - Unmark all nodes.
 * - Scan all roots.
 * - Scan all marked nodes.
 * - Sweep all unmarked nodes.
 * - Sweep all unused blocks.
 * .
 */
void _tm_gc_full_type_inner(tm_type *type)
{
  int try = 0;

  tm_msg("gc {\n");

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_gc_inner);
#endif

  _tm_sweep_is_error = 0;

  /* Try this twice: */
  while ( try ++ < 1 ) {
    /*! Unmark all marked nodes. */
    _tm_phase_init(tm_UNMARK);
    _tm_node_unmark_all();
    tm_assert_test(tm.n[BLACK] == 0);

    /*! Scan all roots, mark pointers. */
    _tm_phase_init(tm_ROOT);
    _tm_root_scan_all();
    
    /*! Scan all marked nodes. */
    _tm_phase_init(tm_SCAN);
    _tm_node_scan_all();
    tm_assert_test(tm.n[GREY] == 0);
    
    /*! Sweep the unmarked nodes. */
    _tm_phase_init(tm_SWEEP);
    _tm_node_sweep_all();
    tm_assert_test(tm.n[ECRU] == 0);
    
    /*! Unmark all marked nodes. */
    _tm_phase_init(tm_UNMARK);
    _tm_node_unmark_all();
    tm_assert_test(tm.n[BLACK] == 0);

    /*! Possibly sweep some blocks. */
    while ( tm_block_sweep_some() ) {
    }

    /*! Start tm_ALLOC phase. */
    _tm_phase_init(tm_ALLOC);
  }

  /*! Clear GC stats. */
  tm_gc_clear_stats();

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_gc_inner);
#endif

  tm_msg("gc }\n");
}


/**
 * Force a full GC sequence.
 */
void _tm_gc_full_inner()
{
  _tm_gc_full_type_inner(0);
}


/*@}*/

/***************************************************************************/
/*! \defgroup allocation Allocation */
/*@{*/


/*
 * Returns true if there is memory pressure for a given tm_type.
 *
 * - There are no free tm_nodes for the type, and there is no block being parceled, and there are no tm_blocks in the free list.
 * - OR the number of in-use nodes vs parcelled nodes is greater than tm_GC_
 */
static __inline
int _tm_type_memory_pressureQ(tm_type *t)
{
  return 
    (! t->n[WHITE] && ! t->parcel_from_block && ! tm.free_blocks_n)
    // || ((t->n[tm_TOTAL] - t->n[tm_FREE]) > t->n[tm_TOTAL] * tm_GC_THRESHOLD)
    // || ((tm.n[tm_TOTAL] - t->n[tm_FREE]) > tm.n[tm_TOTAL] * tm_GC_THRESHOLD)
    ;
}

/*
 * Returns true if there is memory pressure for a given tm_type.
 *
 * - There are no free tm_nodes for the type, and there is no block being parceled, and there are no tm_blocks in the free list.
 * - OR the number of in-use nodes vs parcelled nodes is greater than tm_GC_
 */
static __inline
int _tm_type_memory_pressureQ_2(tm_type *t)
{
  return 
    _tm_type_memory_pressureQ(t) &&
    tm.alloc_since_sweep > 100
    ;
}


/**
 * Allocates a node of a given type.
 *
 * Algorithm:
 *
 * Depending on tm.phase, the allocator will do some other work
 * before returning an data ptr from a tm_node:
 * - Unmark some allocated nodes (BLACK->ECRU).
 * - Scan roots (stacks or globals) for pointers to nodes.
 * - Scan marked nodes for pointers to other nodes (GREY->BLACK).
 * - Sweep some unused nodes to their free lists (ECRU->WHITE).
 * .
 *
 * A node is taken from the tm_type's free list, or a new
 * block is allocated and parcelled into the tm_type's free list.
 *
 */
void *_tm_alloc_type_inner(tm_type *t)
{
  void *ptr;
  int again;
#if tm_TIME_STAT
  tm_time_stat *ts;
#endif

  /*! Increment the allocation id. */
  ++ tm.alloc_id;
#if 0
  /* DEBUG DELETE ME! */
  if ( tm.alloc_id == 5040 )
    tm_stop();
#endif

  /*! Reset during tm_alloc() stats: tm_data.alloc_n. */
  tm.alloc_pass = 0;
  memset(tm.alloc_n, 0, sizeof(tm.alloc_n));

  /*! Keep track of how many allocs since last sweep. */
  ++ tm.alloc_since_sweep;

  // tm_validate_lists();

  /* Try to allocate again? */
  ++ tm.alloc_pass;

  /* BEGIN CRITICAL SECTION */

#if tm_TIME_STAT
  tm_time_stat_begin(ts = &tm.p.ts_phase[tm.p.phase]);
#endif

 again:
  again = 0;

  /*! Assume next allocation phase will be same as current phase. */
  tm.p.next_phase = (enum tm_phase) -1;
  
  /*! If current allocation phase is: */
  switch ( tm.p.phase ) {
  case tm_ALLOC:
    /*! - tm_ALLOC: Allocate ECRU nodes from WHITE free lists. */

    _tm_node_unmark_some(tm_node_unmark_some_size);

    /**
     * If there are no free nodes for the type,
     * Or if there are none to parcel.
     */
    if ( _tm_type_memory_pressureQ_2(t) ) {
      tm.p.next_phase = tm_UNMARK;
    }
    break;

  case tm_UNMARK:
    /*! - tm_UNMARK: Unmark some BLACK nodes. */

    /*! If all nodes are unmarked, next phase is tm_ROOT. */
    if ( ! _tm_node_unmark_some(tm_node_unmark_some_size) ) {
      tm.p.next_phase = tm_ROOT;
    }

#if 0
    /**
     * Begin tm_ROOT phase immediately if memory pressure is still too high.
     */
    if ( _tm_type_memory_pressureQ(t) ) {
      tm.p.next_phase = tm_ROOT;
    }
#endif
    break;
    
  case tm_ROOT:
    /*! - tm_ROOT: scan some roots. */

    _tm_node_scan_some(tm_node_scan_some_size);

    /*! If tm_root_scan_full, */
    if ( tm_root_scan_full ) {
      /*! Scan all roots for pointers, next phase is tm_SCAN. */
      _tm_root_scan_all();
      tm.p.next_phase = tm_SCAN;
    } else {
      /*! Otherwise, scan some roots for pointers. */
      if ( ! _tm_root_scan_some() ) {
	/*! If there were no roots scanned, next phase is tm_SCAN. */
	tm.p.next_phase = tm_SCAN;
      }
    }
    break;
    
  case tm_SCAN:
    /* - tm_SCAN: Scan some tm_GREY nodes for internal pointers. */

    /*! If scanning is complete, */
    if ( ! _tm_node_scan_some(tm_node_scan_some_size) ) {
      /* Scan all roots once more. */
      _tm_root_scan_all();
    }

    /**
     * If still left to scan and under pressure,
     * scan all grey nodes. 
     */
    if ( tm.n[GREY] && _tm_type_memory_pressureQ_2(t) ) {
      _tm_node_scan_all();
    }

    if ( ! tm.n[GREY] ) {
      tm.p.next_phase = tm_SWEEP;
      again = 1;
    }
    break;
      
  case tm_SWEEP:
    /**
     * - tm_SWEEP: reclaim any tm_ECRU nodes.
     *
     * Do not remark nodes after root mutations
     * because all newly allocated objects are marked "in use" during SWEEP.
     * (See tm_phase_alloc).
     */

    /**
     * Try to immediately sweep tm_ECRU nodes for the type request,
     * otherwise try to immediately sweep nodes for any type.
     */
    if ( t->n[tm_TOTAL] && ! _tm_node_sweep_some_for_type(t, tm_node_sweep_some_size) ) {
      _tm_node_sweep_some(tm_node_sweep_some_size);
    }
    
    /*! If no ECRU (unused) nodes are left to sweep, */
    if ( ! tm.n[ECRU] ) { 
      /*! Switch to allocating ECRU nodes from free lists or from OS. */
      tm.p.next_phase = tm_ALLOC;
    }
    break;

  default:
    tm_abort();
    break;
  }
  
  /* END CRITICAL SECTION */

  /*! Switch to new phase. */
  if ( tm.p.next_phase != (enum tm_phase) -1 )
    _tm_phase_init(tm.p.next_phase);

  if ( again )
    goto again;

  /*! Increment the number tm_node allocs per phase. */
  ++ tm.p.alloc_by_phase[tm.p.phase];

  /*! If a node is not available on the tm_type free list, */
  if ( ! (ptr = tm_type_alloc_node_from_free_list(t)) ) {
    /**
     * Attempt to parcel or allocate some nodes:
     * - from currently parceled block, 
     * - a free block or,
     * - from new block from OS. */
    tm_type_parcel_or_alloc_node(t);
    ptr = tm_type_alloc_node_from_free_list(t);
  }

  // tm_validate_lists();

#if tm_ptr_to_node_TEST
  /* Validate tm_ptr_to_node() */
  if ( ptr ) {
    char *p = ptr;
    tm_node *n = (void*) (p - tm_node_HDR_SIZE);
    tm_assert(tm_ptr_to_node(n) == 0);
    tm_assert(tm_ptr_to_node(p) == n);
    tm_assert(tm_ptr_to_node(p + 1) == n);
    tm_assert(tm_ptr_to_node(p + t->size / 2) == n);
    tm_assert(tm_ptr_to_node(p + t->size - 1) == n);
#if tm_ptr_AT_END_IS_VALID
    tm_assert(tm_ptr_to_node(p + t->size) == n);
#else
    tm_assert(tm_ptr_to_node(p + t->size) == 0);
#endif
  }
#endif

#if tm_TIME_STAT
  tm_time_stat_end(ts);
#endif

  tm_alloc_log(ptr);

#if 0
  tm_msg("a %p[%lu]\n", ptr, (unsigned long) t->size);
#endif

  /*! Return a new data ptr or 0. */
  return (void*) ptr;
}


/**
 * Manually returns a node back to its tm_type free list.
 *
 * Assumes that ptr is pure pointer to the beginning of a tm_node's data space.
 */
void _tm_free_inner(void *ptr)
{
  tm_block *b;
  tm_node *n;

  n = tm_pure_ptr_to_node(ptr);
  b = tm_node_to_block(n);
  // _tm_block_validate(b);
  tm_assert(tm_node_color(n) != WHITE);
  tm_node_set_color(n, b, WHITE);
}


/***************************************************************************/


/***************************************************************************/
/* \defgroup error_sweep Error: Sweep */
/*@{*/


#ifndef _tm_sweep_is_error
/*! If true and sweep occurs, raise a error. */
int _tm_sweep_is_error = 0;
#endif

/**
 * Checks for a unexpected node sweep.
 *
 * If a sweep occured at this time,
 * the mark phase created a free tm_node (WHITE) when it should not have.
 *
 * TM failed to mark all tm_nodes as in-use (GREY, or BLACK).
 *
 * This can be considered a bug in:
 * - The write barrier.
 * - Mutators that failed to use the write barrier.
 * - Locating nodes for potential pointers.
 * - Other book-keeping or list management.
 */
int _tm_check_sweep_error()
{
  tm_type *t;
  tm_node *n;

  /*! OK: A sweep is not considered an error. */
  if ( ! _tm_sweep_is_error ) {
    return 0;
  }

  /*! OK: Nothing was left unmarked. */
  if ( ! tm.n[ECRU] ) {
    return 0;
  }

  /*! ERROR: There were some unmarked (ECRU) tm_nodes. */
  tm_msg("Fatal %lu dead nodes; there should be no sweeping.\n", tm.n[ECRU]);
  tm_stop();
  // tm_validate_lists();
  
  /*! Print each unmarked node. */
  tm_list_LOOP(&tm.types, t);
  {
    tm_list_LOOP(&t->color_list[ECRU], n);
    {
      tm_assert_test(tm_node_color(n) == ECRU);
      tm_msg("Fatal node %p color %s size %lu should not be sweeped!\n", 
	     (void*) n, 
	     tm_color_name[tm_node_color(n)], 
	     (unsigned long) t->size);
      {
	void ** vpa = tm_node_to_ptr(n);
	tm_msg("Fatal cons (%d, %p)\n", ((long) vpa[0]) >> 2, vpa[1]);
      }
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;
  
  /*! Print stats. */
  tm_print_stats();
  
  /*! Attempt to mark all roots. */
  _tm_phase_init(tm_ROOT);
  _tm_root_scan_all();
  
  /*! Scan all marked nodes. */
  _tm_phase_init(tm_SCAN);
  _tm_node_scan_all();
  
  /*! Was there still unmarked nodes? */
  if ( tm.n[ECRU] ) {
    tm_msg("Fatal after root mark: still missing %lu references.\n",
	   (unsigned long) tm.n[ECRU]);
  } else {
    tm_msg("Fatal after root mark: OK, missing references found!\n");
  }
  
  /*! Mark all the ECRU nodes BLACK */
  tm_list_LOOP(&tm.types, t);
  {
    tm_list_LOOP(&t->color_list[ECRU], n);
    {
      tm_node_set_color(n, tm_node_to_block(n), BLACK);
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;
  
  /*! Start tm_UNMARK phase. */
  _tm_phase_init(tm_UNMARK);
  
  /*! Assert there is no unmarked nodes. */
  tm_assert_test(tm.n[ECRU] == 0);
  
  /*! Stop in debugger? */
  tm_stop();
  
  /*! Clear worst alloc time. */
  memset(&tm.ts_alloc.tw, 0, sizeof(tm.ts_alloc.tw));
  
  /*! Return 1 to signal an error. */
  return 1;
}


/*@}*/

void tm_node_mutation(tm_node *n)
{
  tm_node_set_color(n, tm_node_to_block(n), GREY);
}

/**
 * Scan node interiors for some pointers.
 *
 * Amount is in pointer-aligned words.
 */
size_t _tm_node_scan_some(size_t amount)
{
  size_t count = 0, bytes = 0;
#define gi (&tm.node_color_iter[GREY])

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
       * Move the tm_GREY node to the marked (BLACK) list.
       */
      tm_node_set_color(gi->scan_node, tm_node_to_block(gi->scan_node), BLACK);

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
      tm_assert_test(tm_node_color(n) == GREY);

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
    tm_msg("M c%lu b%lu l%lu\n", count, bytes, tm.n[GREY]);
#endif

  /*! Return true if there are remaining GREY nodes or some node is still being scanned. */
  return tm.n[GREY] || gi->scan_size;
}

/**
 * Scan until all GREY nodes are BLACK.
 */
void _tm_node_scan_all()
{
  tm_node_LOOP_INIT(GREY);
  while ( _tm_node_scan_some(~ 0UL) ) {
  }
}

#undef gi


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
   * Move the GREY node to the marked (BLACK) list.
   */
  tm_node_set_color(n, tm_node_to_block(n), BLACK);

#if 0
  fprintf(stderr, "[B]");
  fflush(stderr);
#endif
}

#endif
