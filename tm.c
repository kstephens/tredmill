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
The tm_SWEEP phase allocated nodes from tm_WHITE to tm_GREY, because tm_ECRU nodes are considered to be unmarked garbage during the tm_SWEEP phase and tm_BLACK nodes are considered “in-use’ but are not scanned for interior pointers during the tm_SWEEP phase.
Using tm_GREY during tm_SWEEP is also related to tm_BLACK node mutation, which gives rise to requiring a write barrier on tm_BLACK nodes.

The tm_SWEEP phase will always attempt to scan any tm_GREY nodes before continuing to sweep any tm_ECRU nodes.

The tm_BLACK color might seem a better choice for tm_SWEEP allocations, but this would force young nodes, which are more likely to be garbage, to be kept until the next tm_SWEEP phase. 
Coloring tm_SWEEP allocations tm_BLACK would also prevent any new interior pointers stored in nodes that may reference tm_ECRU nodes from being scanned before tm_SWEEP is complete.

If once tm_SWEEP is complete, tm_blocks with no active tm_nodes (i.e. b->n[tm_WHITE] == b->n[tm_TOTAL]) have all their tm_nodes unparcelled and the tm_block is returned to the operating system.

To prevent thrashing the operating system with tm_block allocation and free requests, a limited number of unused blocks are kept on a global tm_block free list.

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

\subsection write_barriers Write Barriers

During the tm_SCAN and tm_SWEEP phase, any tm_BLACK node that is mutated must be rescanned due to the possible introduction of new references from the tm_BLACK node to tm_ECRU nodes. This is achieved by calling tm_write_barrier(&R) in the mutator after modifying R’s contents.

There are two versions of the write barrier:

-# tm_write_barrier(R)
-# tm_write_barrier_pure(R)

tm_write_barrier_pure(R) can be called when R is guaranteed to be a pointer to the head of a node allocated by tm_alloc(). tm_write_barrier_pure(R) cannot be used if R might be an interior pointer or a pointer to a stack or root-allocated object. tm_write_barrier(R) should be used if the address of R is not known to be a pointer to the heap, the stack or global roots.
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

\subsection references References

-# The Treadmill: Real-Time Garbage Collection Without Motion Sickness, Henry G. Baker, http://home.pipeline.com/~hbaker1/NoMotionGC.html

 
 */

#include "tm.h"
#include "internal.h"


/****************************************************************************/
/*! \defgroup validation Validation */
/*@{*/

void tm_validate_lists();


/*@}*/

/**************************************************************************/
/*! \defgroup color Color */
/*@{*/

/*
This table defines what color a newly allocated node
should be during the current allocation phase.
*/

#if 1
/**
 * Default allocated nodes to "not marked".
 * Rational: some are allocated but thrown away early.
 */
#define tm_DEFAULT_ALLOC_COLOR tm_ECRU
#else
/**
 * Default allocated nodes to "scheduled, not marked".
 * Rational: they are allocated because they are to be used (referred to).
 * This seems to cause it to keep allocating new pages before sweeping.
 */
#define tm_DEFAULT_ALLOC_COLOR tm_GREY
#endif


#if 1
/**
 * Default allocated node color during sweep.
 * Rational: Assume that nodes are in-use and may contain pointers 
 * to prevent accidental sweeping.
 */
#define tm_SWEEP_ALLOC_COLOR tm_GREY
#else
#define tm_SWEEP_ALLOC_COLOR tm_BLACK
#endif

static const tm_color tm_phase_alloc[] = {
  tm_DEFAULT_ALLOC_COLOR,  /* tm_ALLOC */
  tm_DEFAULT_ALLOC_COLOR,  /* tm_UNMARK */
  tm_DEFAULT_ALLOC_COLOR,  /* tm_ROOT */
  tm_DEFAULT_ALLOC_COLOR,  /* tm_SCAN */
  tm_SWEEP_ALLOC_COLOR,    /* tm_SWEEP */
  -1
};

#undef tm_DEFAULT_ALLOC_COLOR


/*@}*/

/**************************************************************************/
/*! \defgroup internal_data Internal: Data */
/*@{*/

/**
 * Global allocator data.
 */
struct tm_data tm;


/*@}*/

/****************************************************************************/
/*! \defgroup phase Phase */
/*@{*/


/**
 * Initialize the collection/allocation phase.
 */
void _tm_phase_init(int p)
{
  tm_msg("p %s\n", tm_phase_name[p]);

#if 0
  /* DEBUG: DELETE ME! */
  if ( tm.alloc_id == 1987 )
    tm_stop();
#endif

  ++ tm.n_phase_transitions[tm.phase][p];
  ++ tm.n_phase_transitions[tm.phase][tm_phase_END];
  ++ tm.n_phase_transitions[tm_phase_END][p];
  ++ tm.n_phase_transitions[tm_phase_END][tm_phase_END];

  switch ( (tm.phase = p) ) {
  case tm_ALLOC:
    /**
     * - tm_ALLOC: allocate nodes until memory pressure is high.
     */
    tm.allocating = 1;
    tm.marking = 0;
    tm.scanning = 0;
    tm.sweeping = 0;
    tm.unmarking = 0;

    /*! Set up write barrier hooks. */
    _tm_write_root = __tm_write_root_ignore;
    _tm_write_barrier = __tm_write_barrier_ignore;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
    break;

  case tm_UNMARK:
    /**
     * - tm_UNMARK: begin unmarking tm_BLACK nodes as tm_ECRU:
     */
    tm.allocating = 0;
    tm.marking = 0;
    tm.scanning = 0;
    tm.sweeping = 0;
    tm.unmarking = 1;

    /*! Set up for unmarking. */
    // tm_node_LOOP_INIT(tm_BLACK);

    /*! Keep track of how many tm_BLACK nodes are in use. */
    tm.n[tm_NU] = tm.n[tm_BLACK];

    /*! Set up write barrier hooks. */
    _tm_write_root = __tm_write_root_ignore;
    _tm_write_barrier = __tm_write_barrier_ignore;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
    break;

  case tm_ROOT:
    /**
     * - tm_ROOT: begin scanning roots for potential pointers:
     */
    tm.allocating = 0;
    tm.marking = 1;
    tm.scanning = 0;
    tm.sweeping = 0;
    tm.unmarking = 0;

    /*! Mark stack and data roots as un-mutated. */
    tm.stack_mutations = tm.data_mutations = 0;

    /*! Initialize root mark loop. */
    _tm_root_loop_init();

    /*! Set up write barrier hooks. */
    _tm_write_root = __tm_write_root_root;
    _tm_write_barrier = __tm_write_barrier_root;
    _tm_write_barrier_pure = __tm_write_barrier_pure_root;
    break;

  case tm_SCAN:
    /**
     * - tm_SCAN: begin scanning tm_GREY nodes for internal pointers:
     */
    tm.allocating = 0;
    tm.marking = 1;
    tm.scanning = 1;
    tm.sweeping = 0;
    tm.unmarking = 0;

    /*! Mark stack and data roots as un-mutated. */
    tm.stack_mutations = tm.data_mutations = 0;

    /*! Set up for marking. */
    // tm_node_LOOP_INIT(tm_GREY);

    /*! Set up write barrier hooks. */
    _tm_write_root = __tm_write_root_mark;
    _tm_write_barrier = __tm_write_barrier_mark;
    _tm_write_barrier_pure = __tm_write_barrier_pure_mark;
    break;

  case tm_SWEEP:
    /**
     * - tm_SWEEP: begin reclaiming any ECRU nodes as WHITE.
     */
    tm.allocating = 0;
    tm.marking = 1;
    tm.scanning = 1;
    tm.sweeping = 1;
    tm.unmarking = 0;

    tm_assert_test(tm.n[tm_GREY] == 0);

    /* Set up for scanning. */
    // tm_node_LOOP_INIT(tm_GREY);

    /* Set up for sweeping. */
    // tm_node_LOOP_INIT(tm_ECRU);

    /*! Set up write barrier hooks: */
    /*! Recolor mutated BLACK nodes to GREY for rescanning. */
    _tm_write_root = __tm_write_root_sweep;
    _tm_write_barrier = __tm_write_barrier_sweep;
    _tm_write_barrier_pure = __tm_write_barrier_pure_sweep;
    break;

  default:
    tm_fatal();
    break;
  }

  if ( 1 || p == tm_SWEEP ) {
    // tm_print_stats();
  }
  //tm_validate_lists();
}


/*@}*/


/**
 * Begin sweeping of tm_blocks.
 *
 * UNIMPLEMENTED!
 */
__inline
void _tm_block_sweep_init()
{
#if 0
  tm.bt = tm_list_next(&tm.types);
  tm.bb = tm.bt != tm_list_next(tm.bt) ?
    tm_list_next(&tm.bt->blocks) :
    0;
#endif
}


/***************************************************************************/
/* \defgroup color Color */
/*@{*/


/**
 * Sets the color of a tm_node.
 *
 */
static __inline
void _tm_node_set_color(tm_node *n, tm_block *b, tm_type *t, tm_color c)
{
  int nc = tm_node_color(n);

  tm_assert_test(b);
  tm_assert_test(t);
  tm_assert_test(c <= tm_BLACK);

  /*! Increment cumulative global stats. */
  ++ tm.alloc_n[c];
  ++ tm.alloc_n[tm_TOTAL];

  /*! Increment global color transitions. */
  if ( ! tm.parceling ) {
    tm_assert_test(c != nc);
    ++ tm.n_color_transitions[nc][c];
    ++ tm.n_color_transitions[nc][tm_TOTAL];
    ++ tm.n_color_transitions[tm_TOTAL][c];
    ++ tm.n_color_transitions[tm_TOTAL][tm_TOTAL];
  }

  /*! Adjust global stats. */
  ++ tm.n[c];
  tm_assert_test(tm.n[nc]);
  -- tm.n[nc];

  /*! Adjust type stats. */
  ++ t->n[c];
  tm_assert_test(t->n[nc]);
  -- t->n[nc];

  /*! Adjust block stats. */
  ++ b->n[c];
  tm_assert_test(b->n[nc]);
  -- b->n[nc];

  /*! Set the tm_code color. */
  tm_list_set_color(n, c);
}


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

/**************************************************/
/*! \defgroup block Block */
/*@{*/


/**
 * Add a tm_block to a tm_type.
 */
static __inline
void _tm_type_add_block(tm_type *t, tm_block *b)
{
  tm_assert_test(t);
  tm_assert_test(b);
  /*! Assert that tm_block is not already associated with a tm_type. */
  tm_assert_test(b->type == 0);

  /*! Associate tm_block with the tm_type. */
  b->type = t;

  /*! Increment type block stats. */
  ++ t->n[tm_B];

  /*! Decrement global block stats. */
  ++ tm.n[tm_B];

  /*! Add to type's block list. */
  tm_list_insert(&t->blocks, b);
}


/**
 * Remove a tm_block from its tm_type.
 */
static __inline
void _tm_type_remove_block(tm_type *t, tm_block *b)
{
  tm_assert_test(t);
  tm_assert_test(b->type);
  tm_assert_test(b->type == t);

  /*! Decrement type block stats. */
  tm_assert_test(t->n[tm_B]);
  -- t->n[tm_B];

  /*! Decrement global block stats. */
  tm_assert_test(tm.n[tm_B]);
  -- tm.n[tm_B];

  /*! Do not parcel nodes from it any more. */
  if ( t->parcel_from_block == b ) {
    t->parcel_from_block = 0;
  }

  /*! Remove tm-block from tm_type.block list. */
  tm_list_remove(b);

  /*! Dissassociate tm_block with the tm_type. */
  b->type = 0;
}


/**
 * Initialize a new tm_block.
 */
static __inline
void tm_block_init(tm_block *b)
{
  tm_assert_test(b->size);

  /*! Initialize tm_block list pointers. */
  tm_list_init(&b->list);
  /*! Mark tm_block as tm_LIVE_BLOCK. */
  tm_list_set_color(b, tm_LIVE_BLOCK);

#if tm_name_GUARD
  b->name = "BLOCK";
#endif

  /*! Disassocate the tm_block with any tm_type. */
  b->type = 0;

  /*! Initialize bounds of the tm_block's allocation space. */
  b->begin = (char*) b + tm_block_HDR_SIZE;
  b->end   = (char*) b + b->size;
  
  /*! Initialize next tm_node parcel to beginning of valid useable space. */
  b->next_parcel = b->begin;

#if tm_block_GUARD
  b->guard1 = b->guard2 = tm_block_hash(b);
#endif

  /*! Clear tm_block stats. */
  memset(b->n, 0, sizeof(b->n));

  /*! Reset tm_block sweep iterator. */
  _tm_block_sweep_init();

  /*! Remember the first block and most recent blocks allocated. */
  tm.block_last = b;
  if ( ! tm.block_first ) {
    tm.block_first = b;
  } else {
#if tm_USE_SBRK
    /* Make sure heap grows up, other code depends on it. */
    tm_assert((void*) b >= (void*) tm.block_first);
#endif
  }
}


/**
 * Allocate a tm_block of a given size.
 */
static
tm_block *_tm_block_alloc(size_t size)
{
  tm_block *b = 0;
  size_t offset;

  /*! Force allocation to a multiple of tm_block_SIZE. */
  if ( (offset = (size % tm_block_SIZE)) )
    size += tm_block_SIZE - offset;

  /*! Scan global tm_block free list for a block of the right size. */
  {
    tm_block *bi;

    tm_list_LOOP(&tm.free_blocks, bi);
    {
      if ( bi->size == size &&
	   tm_list_color(bi) == tm_FREE_BLOCK
	   ) {
	tm_assert_test(tm.free_blocks_n);
	-- tm.free_blocks_n;

	tm_list_remove(bi);

	b = bi;

	tm_msg("b a fl b%p %d\n", (void*) b, tm.free_blocks_n);
	break;
      }
    }
    tm_list_LOOP_END;
  }

  /** 
   * Otherwise, allocate a new tm_block from the OS.
   */
  if ( ! b ) {
    /*! Make sure it's aligned to tm_block_SIZE. */
    b = _tm_os_alloc_aligned(size);

    /*! Initialize its size. */
    b->size = size;
    
    /*! Increment OS block stats. */
    ++ tm.n[tm_B_OS];
    if ( tm.n[tm_B_OS_M] < tm.n[tm_B_OS] )
      tm.n[tm_B_OS_M] = tm.n[tm_B_OS];

    tm.n[tm_b_OS] += size;
    if ( tm.n[tm_b_OS_M] < tm.n[tm_b_OS] )
      tm.n[tm_b_OS_M] = tm.n[tm_b_OS];

    tm_msg("b a os b%p\n", (void*) b);
  }

  /*! Initialize the tm_block. */
  tm_block_init(b);

  /*! Increment global block stats. */
  ++ tm.n[tm_B];
  tm.blocks_allocated_since_gc += size / tm_block_SIZE;

  // tm_validate_lists();

  // fprintf(stderr, "  _tm_block_alloc(%p)\n", b);

  /*! Return the new or reused tm_block. */
  return b;
}


/**
 * Scavenges all tm_types for an unused tm_block.
 *
 * NOT USED!
 */
static 
tm_block *tm_block_scavenge(tm_type *t)
{
  tm_type *type = 0;

  tm_list_LOOP(&tm.types, type);
  {
    tm_block *block = 0;

    tm_list_LOOP(&type->blocks, block);
    {
      if ( block->type != t && 
	   tm_block_unused(block) &&
	   block->n[tm_TOTAL]
	   ) {
	return block;
      }
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;

  return 0;
}


/**********************************************************/
/* Block free */

static __inline
void _tm_node_delete(tm_node *n, tm_block *b);

/**
 * Unparcels the tm_nodes in a tm_block.
 *
 * - Removes each tm_node allocated from the tm_block from
 * its WHITE list.
 * - All tm_nodes in the tm_block must be WHITE.
 */
static
int _tm_block_unparcel_nodes(tm_block *b)
{
  int count = 0, bytes = 0;
  tm_type *t = b->type;
  
  tm_assert_test(b->type);

  /**
   * If all nodes in the block are free, 
   * remove all nodes in the block from any lists.
  */
  tm_assert_test(tm_block_unused(b));

  {
    tm_node *n;
    
    /*! Start at first tm_node in tm_block. */
    n = tm_block_node_begin(b);
    while ( (void*) n < tm_block_node_next_parcel(b) ) {
      /*! Remove node from tm_WHITE list and advance. */
      ++ count;
      bytes += b->size;

      tm_assert_test(tm_node_color(n) == tm_WHITE);
      _tm_node_delete(n, b);

      n = tm_block_node_next(b, n);
    }
  }

  /*! Decrement tm_WHITE and tm_TOTAL counts: */

  /*! - Decrement type node counts. */
  tm_assert_test(t->n[tm_WHITE] >= count);
  t->n[tm_WHITE] -= count;
  tm_assert_test(t->n[tm_TOTAL] >= count);
  t->n[tm_TOTAL] -= count;

  /*! - Decrement block node counts. */
  tm_assert_test(b->n[tm_WHITE] >= count);
  b->n[tm_WHITE] -= count;
  tm_assert_test(b->n[tm_WHITE] == 0);

  tm_assert_test(b->n[tm_TOTAL] >= count);
  b->n[tm_TOTAL] -= count;
  tm_assert_test(b->n[tm_TOTAL] == 0);

  /*! - Decrement global node counts. */
  tm_assert_test(tm.n[tm_WHITE] >= count);
  tm.n[tm_WHITE] -= count;
  tm_assert_test(tm.n[tm_TOTAL] >= count);
  tm.n[tm_TOTAL] -= count;

  return count;
}


/**
 * Reclaim a live tm_block.
 */
static __inline
void _tm_block_reclaim(tm_block *b)
{
  tm_assert_test(b);
  tm_assert_test(tm_list_color(b) == tm_LIVE_BLOCK);

  /*! Unparcel any allocated nodes from type free lists. */
  _tm_block_unparcel_nodes(b);

  /*! Avoid pointers into block. */
  b->next_parcel = b->begin;

  /*! Decrement global block stats. */
  tm_assert_test(tm.n[tm_B]);
  -- tm.n[tm_B];

  /*! Remove reference from tm.block_first and tm.block_last, if necessary. */
  if ( tm.block_last == b ) {
    tm.block_last = 0;
  }
  if ( tm.block_first == b ) {
    tm.block_first = 0;
  }

  /*! Remove tm_block from tm_type. */
  _tm_type_remove_block(b->type, b);

  /*! Mark tm_block's pages as unused. */
  _tm_page_mark_unused_range(b, b->size);
}


/**
 * Frees a block either returning the block to the OS or keeping it on a free list.
 *
 * - If using mmap(), the block is returned to the OS only if there are enough blocks on the free list (see: tm_block_min_free).
 * - If using sbrk(), the block is returned to the OS only if it was the most recient block allocated by sbrk().
 *
 */
static __inline
void _tm_block_free(tm_block *b)
{
  int os_free = 0;

  tm_assert(tm_block_unused(b));

  /*! Reclaim tm_block from its tm_type.blocks list. */
  _tm_block_reclaim(b);

#if tm_USE_MMAP
  /*! If using mmap(), reduce calls to _tm_os_free() by keeping tm_block_min_free free blocks. */
  if ( tm.free_blocks_n > tm_block_min_free ) {
    // fprintf(stderr, "  tm_block_free too many free blocks: %d\n", tm.free_blocks_n);
    os_free = 1;
  } else {
    os_free = 0;
  }
#endif

#if tm_USE_SBRK
  /*! If using sbrk() and b was the last block allocated from the OS, */
  if ( sbrk(0) == (void*) b->end ) {
    /*! Reduce valid node ptr range. */
    tm_assert(tm_ptr_h == b->end);
    tm_ptr_h = b;

    /*! And plan to return block to OS. */
    os_free = 1;
  } else {
    os_free = 0;
  }
#endif

  /*! If block should return to OS, */
  if ( os_free ) {
    /*! Decrement global OS block stats. */
    tm_assert_test(tm.n[tm_B_OS]);
    -- tm.n[tm_B_OS];

    tm_assert_test(tm.n[tm_b_OS] > b->size);
    tm.n[tm_b_OS] -= b->size;

    /*! And return aligned block back to OS. */
    _tm_os_free_aligned(b, b->size);

    tm_msg("b f os b%p\n", (void*) b);
  } else {
    /*! Otherwise, remove from t->blocks list and add to global free block list. */
    tm_list_remove_and_append(&tm.free_blocks, b);
    ++ tm.free_blocks_n;

    /*! Mark block as tm_FREE_BLOCK. */
    tm_list_set_color(b, tm_FREE_BLOCK);

    tm_msg("b f fl b%p %d\n", (void*) b, tm.free_blocks_n);
  }

  /*! Reset block sweep iterator. */
  _tm_block_sweep_init();

  // tm_validate_lists();

  // fprintf(stderr, "  _tm_block_free(%p)\n", b);
  // tm_msg("b f b%p\n", (void*) b);

  /*! Return. */
}


/**
 * Allocates a tm_block of tm_block_SIZE for a tm_type.
 */
static 
tm_block * _tm_block_alloc_for_type(tm_type *t)
{
  tm_block *b;
  
  /*! Allocate a new tm_block. */
  b = _tm_block_alloc(tm_block_SIZE);

  // fprintf(stderr, "  _tm_block_alloc(%d) => %p\n", tm_block_SIZE, b);

  /*! Add it to the tm_type.blocks list */
  if ( b ) {
    _tm_type_add_block(t, b);
  }

  // tm_msg("b a b%p t%p\n", (void*) b, (void*) t);

#if 0
    fprintf(stderr, "  _tm_block_alloc_for_type(%p) => %p\n",
	    (void*) t,
	    (void*) t->parcel_from_block);
#endif

  return b;
}


/*@}*/

/*********************************************************************/
/*! \defgroup node Node */
/*@{*/

/**
 * Initialize a tm_node from a tm_block.
 */
static __inline 
void _tm_node_init(tm_node *n, tm_block *b)
{
  tm_type *t;

  tm_assert_test(b);
  // _tm_block_validate(b);
  tm_assert_test(b->type);

  /*! Set the tm_block.type. */
  t = b->type;

  /*! Initialize its list pointers. */
  tm_list_init(&n->list);

#if 1
  tm_assert_test(tm_list_color(&n->list) == tm_WHITE);
#else
  /*! Set the tm_node color to tm_WHITE. */
  tm_list_set_color(n, tm_WHITE);
#endif

  /*! Increment type node counts. */
  ++ t->n[tm_TOTAL];
  ++ t->n[tm_WHITE];

  /*! Increment block node counts */
  ++ b->n[tm_TOTAL];
  ++ b->n[tm_WHITE];
    
  /*! Increment global node counts. */
  ++ tm.n[tm_TOTAL];
  ++ tm.n[tm_WHITE];
  
  /*! Place tm_node on tm_type tm_WHITE list. */
  tm_node_set_color(n, b, tm_WHITE);
  
  // tm_validate_lists();

  // tm_msg("N n%p t%p\n", (void*) n, (void*) t);
}


/**
 * Deletes a WHITE tm_node from a tm_block.
 */
static __inline 
void _tm_node_delete(tm_node *n, tm_block *b)
{
  tm_type *t;
  tm_color nc = tm_node_color(n);

  // _tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;
  tm_assert_test(nc == tm_WHITE);

#if 0 /* See _tm_block_unparcel_nodes() */
  /*! Decrement type node counts. */
  tm_assert_test(t->n[tm_TOTAL]);
  -- t->n[tm_TOTAL];
  tm_assert_test(t->n[nc]);
  -- t->n[nc];

  /*! Decrement block node counts. */
  tm_assert_test(b->n[tm_TOTAL] > 0);
  -- b->n[tm_TOTAL];
  tm_assert_test(b->n[nc]) > 0);
  -- b->n[nc];

  /*! Decrement global node counts. */
  tm_assert_test(tm.n[tm_TOTAL] > 0);
  -- tm.n[tm_TOTAL];
  tm_assert_test(tm.n[nc] > 0);
  -- tm.n[nc];
#endif

  /*! Remove tm_node from tm_type's color list. */
  tm_list_remove(n);

  // tm_validate_lists();

#if 0
  tm_msg("N d n%p[%lu] t%p\n", 
	 (void*) n,
	 (unsigned long) t->size,
	 (void*) t);
#endif
}


/**
 * Parcel some nodes for a tm_type from a tm_block
 * already allocated from the OS.
 */
static __inline 
int _tm_node_parcel_some(tm_type *t, long left)
{
  int count = 0;
  size_t bytes = 0;
  tm_block *b;
  
  ++ tm.parceling;

  /**
   * If a tm_block is already available for parceling, parcel from it.
   * Otherwise, give up.
   */
  if ( ! (b = t->parcel_from_block) ) {
    goto done;
  }

  // _tm_block_validate(b);
  
  {
    /*! Until end of tm_block parcel space is reached, */
    void *pe = tm_block_node_begin(b);
    
    while ( (pe = tm_block_node_next(b, tm_block_node_next_parcel(b))) 
	    <= tm_block_node_end(b)
	    ) {
      tm_node *n;
        
      // _tm_block_validate(b);
      
      /*! Parcel a tm_node from the tm_block. */
      n = tm_block_node_next_parcel(b);
      
      /*! Increment tm_block parcel pointer. */
      b->next_parcel = pe;
      
      /*! Update global valid node pointer range. */
      {
	void *ptr;

	if ( tm_ptr_l > (ptr = tm_node_ptr(n)) ) {
	  tm_ptr_l = ptr;
	}
	if ( tm_ptr_h < (ptr = pe) ) {
	  tm_ptr_h = ptr;
	}
      }
  
      /*! Initialize the tm_node. */
      _tm_node_init(n, b);

      tm_assert_test(tm_node_color(n) == tm_WHITE);

      /*! Update local accounting. */
      ++ count;
      bytes += t->size;
      
      /*! If enough nodes have been parceled, stop. */
      if ( -- left <= 0 ) {
	goto done;
      }
    }
    
    /** 
     * If the end of the tm_block was reached,
     * Force a new tm_block allocation next time. 
     */
    t->parcel_from_block = 0;
  }

  done:

  -- tm.parceling;

#if 0
  if ( count )
    tm_msg("N i n%lu b%lu t%lu\n", count, bytes, t->n[tm_WHITE]);
#endif

  /*! Return the number of tm_nodes actually allocated. */
  return count;
}


/**
 * Parcel some nodes from an existing tm_block,
 * or allocate a new tm_block and try again.
 */
static __inline
int _tm_node_parcel_or_alloc(tm_type *t)
{
  int count;

  count = _tm_node_parcel_some(t, tm_node_parcel_some_size);
  if ( ! count ) {
    t->parcel_from_block = _tm_block_alloc_for_type(t);
    if ( ! t->parcel_from_block )
      return 0;
    count = _tm_node_parcel_some(t, tm_node_parcel_some_size);
  }

  /*! Return the number of tm_node parcelled. */
  return count;
}


/*@}*/

/************************************************************************/
/*! \defgroup type Type */
/*@{*/

/**
 * Initialize a new tm_type of a given size.
 */
__inline
void tm_type_init(tm_type *t, size_t size)
{
  /*! Assign a the tm_type a unique id. */
  t->id = ++ tm.type_id;

  /*! Initialize tm_type's global list pointers. */
  tm_list_init(&t->list);
  
  /*! Mark the tm_type as live. */
  tm_list_set_color(&t->list, tm_LIVE_TYPE);
#if tm_name_GUARD
  t->name = "TYPE";
#endif
  t->size = size;

  /*! Initialize the tm_types.blocks list. */
  tm_list_init(&t->blocks);
  tm_list_set_color(&t->blocks, tm_LIVE_BLOCK);
  
  /*! Initialize the tm_type node counts. */
  memset(t->n, 0, sizeof(t->n));
  
  /*! Initialize the tm_type.color_list. */
  {
    int j;
    
    for ( j = 0; j < sizeof(t->color_list) / sizeof(t->color_list[0]); ++ j ) {
      tm_list_init(&t->color_list[j]);
      tm_list_set_color(&t->color_list[j], j);
    }
  }

  /*! Force a new tm_block to be allocated and parceled. */
  t->parcel_from_block = 0;

  /*! Zero the tm_type descriptor. */
  t->desc = 0;
}


/**
 * Returns a new tm_type for a given size.
 */
static __inline 
tm_type *tm_type_new(size_t size)
{
  tm_type *t;

  /*! If possible take a tm_type from global tm.type_free list. */
  if ( tm.type_free ) {
    t = tm.type_free;
    tm.type_free = t->hash_next;
    t->hash_next = 0;
  } else {
    t = 0;
  }

  /*! Initialize the tm_type. */
  tm_type_init(t, size);

  /*! Add to global tm.types list. */
  tm_list_insert(&tm.types, t);
  
  /**
   * Allocate some tm_nodes now so the tm_type's free list (tm_WHITE) is not empty.
   * to avoid triggering a collection.
   *
   * Assume this is being called because an tm_alloc() is envitable.
   */
  _tm_node_parcel_or_alloc(t);

  return t;
}


/**
 * Returns the tm.type_hash[] index for a size.
 */
static __inline 
int tm_size_hash(size_t size)
{
  int i;

  /*! IMPLEMENT: support for big allocs. */
  tm_assert(size <= tm_block_SIZE_MAX);

  /*! Compute hash bucket index. */
  i = size / tm_ALLOC_ALIGN;

  /*! Modulus index into hash bucket index. */
  i %= tm_type_hash_LEN;

  return i;
}


/**
 * Look for a type by size.
 */
static __inline 
tm_type *tm_size_to_type_2(size_t size)
{
  tm_type **tp, *t;
  int i;
  
  i = tm_size_hash(size);

  /*! Look for type by size using tm.type_hash. */
  for ( tp = &tm.type_hash[i]; (t = (*tp)); tp = &t->hash_next ) {
    /*! If a type of the same size is found, */
    if ( t->size == size ) {
      /* Move it to front of hash bucket. */
      *tp = t->hash_next;
      t->hash_next = tm.type_hash[i];
      tm.type_hash[i] = t;
      break;
    }
  }

  return t;
}


/**
 * Returns a new tm_type for a given size.
 */
static __inline 
tm_type *tm_type_new_2(size_t size)
{
  int i;
  tm_type *t;

  /*! Allocate a new tm_type. */
  t = tm_type_new(size);
  
  /*! Add to tm.type_hash */
  i = tm_size_hash(t->size);
  t->hash_next = tm.type_hash[i];
  tm.type_hash[i] = t;
  
#if 0
  tm_msg("t a t%p %lu\n", (void*) t, (unsigned long) size);
#endif

  return t;
}


/**
 * WHAT DOES THIS DO???
 */
tm_adesc *tm_adesc_for_size(tm_adesc *desc, int force_new)
{
  tm_type *t;

  if ( ! force_new ) {
    t = tm_size_to_type_2(desc->size);
    if ( t )
      return t->desc;
  }

  t = tm_type_new_2(desc->size);
  t->desc = desc;
  t->desc->hidden = t;

  return t->desc;
}


/**
 * Return a tm_type for a size.
 */
static __inline 
tm_type *tm_size_to_type(size_t size)
{
  tm_type *t;
  
  /*! Align size to tm_ALLOC_ALIGN. */
  size = (size + (tm_ALLOC_ALIGN - 1)) & ~(tm_ALLOC_ALIGN - 1);

  /*! If there already exists a type of the size, return it. */
  t = tm_size_to_type_2(size);

  if ( t )
    return t;

  /*! Otherwise, align size to the next power of two. */
#define POW2(i) if ( size <= (1UL << i) ) size = (1UL << i); else

  POW2(3)
  POW2(4)
  POW2(5)
  POW2(6)
  POW2(7)
  POW2(8)
  POW2(9)
  POW2(10)
  POW2(11)
  POW2(12)
  POW2(13)
  POW2(14)
  POW2(15)
  POW2(16)
  (void) 0;

#undef POW2

  /*! Try to locate a bigger tm_type of that size. */
  t = tm_size_to_type_2(size);

  /* If a tm_type was not found, create a new one. */
  if ( ! t ) {
    t = tm_type_new_2(size);
  }
  
  tm_assert_test(t->size == size);
  
  return t;
}


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
  tm_assert_test(tm_node_color(n) == tm_ECRU);
  // _tm_block_validate(b);

  /*! Clear out the data portion of the tm_node. */
  memset(tm_node_to_ptr(n), 0, b->type->size);

  /*! Put the tm_node on its tm_type free (tm_WHITE) list. */
  tm_node_set_color(n, b, tm_WHITE);

#if 0
  tm_msg("s n%p\n", n);
#endif

  /*! Free the tm_node's tm_block if the tm_block completely unused. */
  if ( tm_block_unused(b) ) {
    _tm_block_free(b);
  }
}


/**
 * Sweep some tm_ECRU nodes of any type.
 */
static 
size_t _tm_node_sweep_some(int left)
{
  size_t count = 0, bytes = 0;
 
  if ( tm.n[tm_ECRU] ) {
    if ( _tm_check_sweep_error() )
      return 0;

    tm_node_LOOP(tm_ECRU);
    {
      tm_assert_test(tm_node_color(n) == tm_ECRU);

      _tm_node_sweep(n, tm_node_to_block(n));

      bytes += t->size;
      ++ count;

      if ( -- left <= 0 ) {
	tm_node_LOOP_BREAK(tm_ECRU);
      }
    }
    tm_node_LOOP_END(tm_ECRU);
  }

  /*! Decrement global stats. */
  tm.n[tm_b] -= bytes;

#if 0
  if ( count ) 
    tm_msg("s c%lu b%lu l%lu\n", count, bytes, tm.n[tm_ECRU]);
#endif

  /*! Return the global number of tm_ECRU nodes. */
  return tm.n[tm_ECRU];
}


/**
 * Sweep some nodes for a tm_type.
 */
static 
size_t _tm_node_sweep_some_for_type(tm_type *t, int left)
{
  size_t count = 0, bytes = 0;
  tm_node *n;

  while ( t->n[tm_ECRU] ) {
    n = tm_list_first(&t->color_list[tm_ECRU]);
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
    tm_msg("s t%p c%lu b%lu l%lu\n", t, count, bytes, tm.n[tm_ECRU]);
#endif

  /*! Return the number of tm_ECRU nodes for the tm_type. */
  return t->n[tm_ECRU];
}


/**
 * Sweep all ECRU nodes back to free lists.
 */
static 
void _tm_node_sweep_all()
{
  /*! While there are tm_ECRU nodes, */
  while ( tm.n[tm_ECRU] ) {
    /*! Keep sweeping them. */
    tm_node_LOOP_INIT(tm_ECRU);
    _tm_node_sweep_some(tm.n[tm_ECRU]);
  }
}


/*@}*/

/***************************************************************************/
/*! \defgroup unmark Unmark */
/*@{*/


/**
 * Maybe sweep a tm_block.
 *
 * NOT USED!
 */
static 
int _tm_block_sweep_maybe(tm_block *b)
{
  return 0;
}


/**
 * Sweep some blocks.
 */
static 
int _tm_block_sweep_some()
{
  int count = 0;
  unsigned long bytes = 0;
  int left = tm_block_sweep_some_size;

#if tm_USE_SBRK
  /*
  ** Try to sweep last block allocated from OS first
  ** because we might be able to return it to the OS.
  */
#endif

  /*! NOT IMPLEMENTED! */

#if 0
  if ( count ) 
    tm_msg("b s b%lu b%lu\n", (unsigned long) count, (unsigned long) bytes);
#endif

  return count;
}


/**
 * Unmark some tm_BLACK nodes.
 */
static 
size_t _tm_node_unmark_some(long left)
{
  size_t count = 0, bytes = 0;

  tm_node_LOOP(tm_BLACK);
  {
    tm_assert_test(tm_node_color(n) == tm_BLACK);
    
    tm_node_set_color(n, tm_node_to_block(n), tm_ECRU);
    
    bytes += t->size;
    ++ count;
    
    if ( -- left <= 0 ) {
      tm_node_LOOP_BREAK(tm_BLACK);
    }
  }
  tm_node_LOOP_END(tm_BLACK);

#if 0
  if ( count ) 
    tm_msg("u n%lu b%lu l%lu\n", count, bytes, tm.n[tm_BLACK]);
#endif

  return tm.n[tm_BLACK];
}


/**
 * Unmark all tm_BLACK nodes.
 */
static 
void _tm_node_unmark_all()
{
  while ( tm.n[tm_BLACK] ) {
    tm_node_LOOP_INIT(tm_BLACK);
    _tm_node_unmark_some(tm.n[tm_TOTAL]);
  }
}


/*@}*/

/***************************************************************************/
/*! \defgroup gc GC */
/*@{*/


/**
 * Clear GC stats.
 */
static
void _tm_gc_clear_stats()
{
  tm.blocks_allocated_since_gc = 0;
  tm.blocks_in_use_after_gc = tm.n[tm_B];

  tm.nodes_allocated_since_gc = 0;
  tm.nodes_in_use_after_gc = tm.n[tm_NU] = tm.n[tm_TOTAL] - tm.n[tm_WHITE];

  tm.bytes_allocated_since_gc = 0;
  tm.bytes_in_use_after_gc = tm.n[tm_b];
}


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

  _tm_sweep_is_error = 0;

  /* Try this twice: */
  while ( try ++ < 2) {
    /*! Unmark all marked nodes. */
    _tm_phase_init(tm_UNMARK);
    _tm_node_unmark_all();
    tm_assert_test(tm.n[tm_BLACK] == 0);

    /*! Scan all roots, mark pointers. */
    _tm_phase_init(tm_ROOT);
    _tm_root_scan_all();
    
    /*! Scan all marked nodes. */
    _tm_phase_init(tm_SCAN);
    _tm_node_scan_all();
    tm_assert_test(tm.n[tm_GREY] == 0);
    
    /*! Sweep the unmarked nodes. */
    _tm_phase_init(tm_SWEEP);
    _tm_node_sweep_all();
    tm_assert_test(tm.n[tm_ECRU] == 0);
    
    /*! Unmark all marked nodes. */
    _tm_phase_init(tm_UNMARK);
    _tm_node_unmark_all();
    tm_assert_test(tm.n[tm_BLACK] == 0);

    /*! Possibly sweep some blocks. */
    while ( _tm_block_sweep_some() ) {
    }

    /*! Start tm_ALLOC phase. */
    _tm_phase_init(tm_ALLOC);
  }

  /*! Clear GC stats. */
  _tm_gc_clear_stats();

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


/**
 * Allocates a node from at tm_type's free list.
 */
static __inline 
void *_tm_type_alloc_node_from_free_list(tm_type *t)
{
  tm_node *n;
  char *ptr;

  /*! If a tm_node is not available on the tm_type's free (tm_WHITE) list, return 0; */
  if ( ! (n = tm_list_first(&t->color_list[tm_WHITE])) ) {
    return 0;
  }

  /*! Otherwise, */

  /*! Assert its located in a valid tm_block. */
  tm_assert_test(tm_node_to_block(n)->type == t);
  
  /*! Assert that is really tm_WHITE. */
  tm_assert_test(tm_node_color(n) == tm_WHITE);
  
  /*! Get the ptr the tm_node's data space. */
  ptr = tm_node_to_ptr(n);
  
  /*! Clear the data space. */
  memset(ptr, 0, t->size);
  
  /*! Put the tm_node on the appropriate allocated list, depending on the tm.phase. */
  tm_node_set_color(n, tm_node_to_block(n), tm_phase_alloc[tm.phase]);
  
  /*! Mark the tm_node's page as used. */
  _tm_page_mark_used(ptr);
  
  /*! Keep track of allocation amounts. */
  ++ tm.nodes_allocated_since_gc;
  tm.bytes_allocated_since_gc += t->size;
  tm.n[tm_b] += t->size;
  
#if 0
  tm_msg("N a n%p[%lu] t%p\n", 
	 (void*) n, 
	 (unsigned long) t->size,
	 (void*) t);
#endif

  /*! Return the pointer to the data space. */
  return ptr;
}


#ifndef tm_GC_THRESHOLD
#define tm_GC_THRESHOLD 3 / 4
#endif

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

  /*! Assume next allocation phase will be same as current phase. */
  tm.next_phase = (enum tm_phase) -1;
  
  // tm_validate_lists();

  /* Try to allocate again? */
  ++ tm.alloc_pass;

  /* BEGIN CRITICAL SECTION */

#if tm_TIME_STAT
  tm_time_stat_begin(ts = &tm.ts_phase[tm.phase]);
#endif

  /*! If current allocation phase is: */
  switch ( tm.phase ) {
  case tm_ALLOC:
    /*! - tm_UNMARK: Allocate tm_ECRU nodes from tm_WHITE free lists. */

    /**
     * If there are no free nodes for the type,
     * Or if there are none to parcel.
     */
    if ( ! t->n[tm_WHITE] 
	 || ! _tm_node_parcel_some(t, tm_node_parcel_some_size)
	 // || (t->n[tm_ECRU] + t->n[tm_BLACK] > t->n[tm_TOTAL] * tm_GC_THRESHOLD)
	 // || (tm.n[tm_ECRU] + tm.n[tm_BLACK] > tm.n[tm_TOTAL] * tm_GC_THRESHOLD)
	 ) {
      tm.next_phase = tm_UNMARK;
    }
    break;

  case tm_UNMARK:
    /*! - tm_UNMARK: Unmark some tm_BLACK nodes. */
    if ( tm.n[tm_BLACK] ) {
      /*! If all nodes are unmarked, next phase is tm_ROOT. */
      if ( ! _tm_node_unmark_some(tm_node_unmark_some_size) ) {
	tm.next_phase = tm_ROOT;
      }
    }

    /**
     * Begin tm_ROOT phase if:
     *
     * The number of nodes allocated is greater than the total nodes * tm_GC_THRESHOLD.
     */
    if ( (! t->n[tm_WHITE]) ||
	 (t->n[tm_phase_alloc[tm.phase]] > t->n[tm_TOTAL] * tm_GC_THRESHOLD) ||
	 (tm.n[tm_phase_alloc[tm.phase]] > tm.n[tm_TOTAL] * tm_GC_THRESHOLD)
	 ) {
      tm.next_phase = tm_ROOT;
    }
    break;
    
  case tm_ROOT:
    /*! - tm_ROOT: scan some roots. */
    /*! If tm_root_scan_full, */
    if ( tm_root_scan_full ) {
      /*! Scan all roots for pointers, next phase is tm_SCAN. */
      _tm_root_scan_all();
      tm.next_phase = tm_SCAN;
    } else {
      /*! Otherwise, scan some roots for pointers. */
      if ( ! _tm_root_scan_some() ) {
	/*! If there were no roots scanned, next phase is tm_SCAN. */
	tm.next_phase = tm_SCAN;
      }
    }
    break;
    
  case tm_SCAN:
    /* - tm_SCAN: Scan some tm_ECRU nodes for internal pointers. */

    /*! If scanning is complete, */
    if ( ! _tm_node_scan_some(tm_node_scan_some_size) ) {
      /* 
      ** And if the roots were mutated during mark,
      ** remark all roots.
      ** Rationale: A reference to a new node may have been
      ** copied from the unmarked stack to a global root.
      */
      if ( tm_root_scan_full || 
	   tm.data_mutations || 
	   tm.stack_mutations ) {
	/*! Then, scan all roots. */
	_tm_root_scan_all();
      } else {
	/* Otherwise, scan the stack. */
	_tm_stack_scan();
      }
      
      /** 
       * If after marking the register and stack,
       * no additional nodes were marked,
       * begin sweeping.
      */
      if ( ! _tm_node_scan_some(tm_node_scan_some_size) ) {
	/*! And force SWEEP phase so new node gets allocated as GREY. */
	_tm_phase_init(tm_SWEEP);
	tm.next_phase = -1;
      }
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

    /* If there are unmarked tm_ECRU nodes, */
    if ( tm.n[tm_ECRU] ) {
      /*! And if there are nodes to be scanned, */
      if ( tm.n[tm_GREY] ) {
	/*! Scan some tm_GREY nodes. */
	_tm_node_scan_some(tm_node_scan_some_size * 2);
	if ( ! tm.n[tm_GREY] ) {
	  /*! If any new nodes became tm_GREY, restart the sweep iterator. */
	  /**
	   * Rational: the tm_GREY nodes may have new references 
	   * to nodes that would otherwise be sweeped.
	   */
	  tm_node_LOOP_INIT(tm_ECRU);
	}
      }

      /*! If there are no tm_GREY nodes after scanning any tm_GREY nodes, */
      if ( ! tm.n[tm_GREY] ) {
	/**
	 * Try to immediately sweep nodes for the type request,
	 * otherwise try to immediately sweep nodes for any type.
	 */
	if ( ! 
	     (
	      _tm_node_sweep_some_for_type(t, tm_node_sweep_some_size) ||
	      _tm_node_sweep_some(tm_node_sweep_some_size)
	      ) 
	     ) {
	  /*! If no nodes were swept, */
	  /*! Allocate nodes from free lists or from OS. */
	  tm.next_phase = tm_ALLOC;
	}
      }
    } else {
      /*! Otherwise, there are no nodes left to sweep: */
      /*! Allocate nodes from free lists or from OS. */
      tm.next_phase = tm_ALLOC;
    }
    break;

  default:
    tm_abort();
    break;
  }
  
  /* END CRITICAL SECTION */

  /*! Switch to new phase. */
  if ( tm.next_phase != (enum tm_phase) -1 )
    _tm_phase_init(tm.next_phase);

  /*! Increment the number tm_node allocs per phase. */
  ++ tm.alloc_by_phase[tm.phase];

  /*! If a node is not available on the tm_type free list, */
  if ( ! (ptr = _tm_type_alloc_node_from_free_list(t)) ) {
    /**
     * Attempt to parcel or allocate some nodes:
     * - from currently parceled block, 
     * - a free block or,
     * - from new block from OS. */
    _tm_node_parcel_or_alloc(t);
    ptr = _tm_type_alloc_node_from_free_list(t);
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

#if 0
  tm_msg("a %p[%lu]\n", ptr, (unsigned long) t->size);
#endif

  /*! Return a new data ptr or 0. */
  return (void*) ptr;
}


/**
 * Allocates a node of a particular size.
 */
void *_tm_alloc_inner(size_t size)
{
  tm_type *type = tm_size_to_type(size);
  tm.alloc_request_size = size;
  tm.alloc_request_type = type;
  return _tm_alloc_type_inner(type);  
}


/**
 * Allocates a node of a particular type.
 */
void *_tm_alloc_desc_inner(tm_adesc *desc)
{
  tm_type *type = (tm_type*) desc->hidden;
  tm.alloc_request_size = type->size;
  tm.alloc_request_type = type;
  return _tm_alloc_type_inner(type);
}


/**
 * Reallocates a node to a particular size.
 */
void *_tm_realloc_inner(void *oldptr, size_t size)
{
  char *ptr = 0;
  tm_type *t, *oldt;

  oldt = tm_node_to_type(tm_pure_ptr_to_node(oldptr));
  t = tm_size_to_type(size);

  if ( oldt == t ) {
    ptr = oldptr;
  } else {
    ptr = _tm_alloc_inner(size);
    memcpy(ptr, oldptr, size < oldt->size ? size : oldt->size);
  }
  
  return (void*) ptr;
}


/*@}*/


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
  tm_assert(tm_node_color(n) != tm_WHITE);
  tm_node_set_color(n, b, tm_WHITE);
}


/***************************************************************************/
/*! \defgroup internal Internal */
/*@{*/

/**
 * Clears some words on the stack to prevent some garabage.
 *
 * Avoid leaving garbage on the stack
 * Note: Do not move this in to user.c, it might be optimized away.
 */
void __tm_clear_some_stack_words()
{
  int some_words[64];
  memset(some_words, 0, sizeof(some_words));
}


/*@}*/
/***************************************************************************/


