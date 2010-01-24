/** \file tm.h
 * \brief TM API.
 *
 * $Id: tm.h,v 1.10 2008-01-15 05:21:03 stephens Exp $
 */
#ifndef _tredmill_tm_h
#define _tredmill_tm_h

/*******************************************************************************/
/* Headers */

#include <stddef.h> /* size_t */
#include <stdio.h> /* FILE */

/*******************************************************************************/
/*! \defgroup configuration Configuration */
/*@{*/

#ifndef tm_ptr_TO_END_IS_VALID
/**
 * If true, a pointer immediately after the end of a node is considered to be "in" the node.
 *
 * For example:
 *   char *ptr = tm_malloc(10);
 *   for ( int i = 0; i < 10; ++ i ) {
 *      *(ptr ++) = i;
 *   }
 *   something_that_calls_tm_malloc();
 *   ptr -= 10;
 */
#define tm_ptr_TO_END_IS_VALID 0
#endif

/*******************************************************************************/

/**
 * Allocation descriptor.
 *
 * Specifies a user-defined handle for requesting allocations of a particular type and size.
 */
typedef struct tm_adesc {
  size_t size;  /*!< Size of all objects allocated. */
  void *opaque; /*!< User-specified data. */
  void *hidden; /*!< tm_type handle. */
} tm_adesc;

/*! ???? */
tm_adesc *tm_adesc_for_size(tm_adesc *desc, int force_new);

/*@}*/

/*******************************************************************************/
/*! \defgroup initialization Initialization */
/*@{*/

void tm_init(int *argcp, char ***argvp, char ***envpp);


/*@}*/

/*******************************************************************************/
/*! \defgroup root_set Root Set */
/*@{*/

int  tm_root_add(const char *name, const void *begin, const void *end);
void tm_root_remove(const char *name, const void *begin, const void *end);


/*@}*/

/*******************************************************************************/
/*! \defgroup allocation Allocation */
/*@{*/

void *tm_alloc(size_t size);
void *tm_alloc_desc(tm_adesc *desc);
void *tm_realloc(void *ptr, size_t size);
void tm_free(void *ptr);


/*@}*/

/*******************************************************************************/
/*! \defgroup write_barrier Write barrier */
/*@}*/

/**
 * Write barrier hook for stack or data segment.
 *
 * Assumes referent is a stack or data segment location.
 */
extern void (*_tm_write_root)(void *referent);
/*! Wrapper around _tm_write_root(). */
#define tm_write_root(X)(*_tm_write_root)(X)

/**
 * Write barrier hook for a pointer to the stack, to a data segment, or within
 * a tm_alloc()'ed node.
 */
extern void (*_tm_write_barrier)(void *referent);
/*! Wrapper around _tm_write_barrier(). */
#define tm_write_barrier(R) (*_tm_write_barrier)(R)

/**
 * Write barrier hook to a tm_alloc()'ed node.
 * 
 * Assumes referent is a tm_malloc() ptr
 */
extern void (*_tm_write_barrier_pure)(void *referent);
/*! Wrapper around _tm_write_barrier_pure(). */
#define tm_write_barrier_pure(R) (*_tm_write_barrier_pure)(R)

/*@}*/

/*******************************************************************************/
/*! \defgroup gc GC */
/*@{*/

/**
 * Force a full GC.
 *
 * May stop world.
 */
void tm_gc_full();


/*@}*/

/*******************************************************************************/
/*! \defgroup debugging Debugging */
/*@{*/

extern int tm_sweep_is_error;

extern FILE *tm_msg_file;
extern const char *tm_msg_prefix;

void tm_msg_enable(const char *codes, int enable);
void tm_msg(const char *format, ...);
void tm_msg1(const char *format, ...);
void tm_print_stats();
void tm_print_block_stats();
void tm_print_time_stats();


/*@}*/

/*******************************************************************************/
/*! \defgroup control Control */
/*@{*/

extern long tm_node_parcel_some_size;
extern long tm_root_scan_some_size;
extern long tm_node_scan_some_size;
extern long tm_node_sweep_some_size;
extern long tm_node_unmark_some_size;
extern long tm_block_sweep_some_size;
extern int tm_block_min_free;
extern size_t tm_os_alloc_max;
extern int tm_root_scan_full;

/*@}*/

/*******************************************************************************/
/* EOF */

#endif
