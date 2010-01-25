/** \file barrier.h
 * \brief Write barriers.
 */
#ifndef tm_BARRIER_H
#define tm_BARRIER_H

/*******************************************************************************/
/*! \defgroup write_barrier Write barrier */
/*@}*/

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


/**
 * Write barrier hook for stack or data segment.
 *
 * Assumes referent is a stack or data segment location.
 */
extern void (*_tm_write_barrier_root)(void *referent);
/*! Wrapper around _tm_write_barrier_root(). */
#define tm_write_barrier_root(X)(*_tm_write_barrier_root)(X)

void __tm_write_barrier(void *referent);
void __tm_write_barrier_pure(void *referent);
void __tm_write_barrier_root(void *referent);
void __tm_write_barrier_ignore(void *referent);

/*@}*/

#endif
