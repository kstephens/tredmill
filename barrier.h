/** \file barrier.h
 * \brief Write barriers.
 */
#ifndef tm_BARRIER_H
#define tm_BARRIER_H

void __tm_write_root_ignore(void *referent);
void __tm_write_root_root(void *referent);
void __tm_write_root_mark(void *referent);
void __tm_write_root_sweep(void *referent);

void __tm_write_barrier_ignore(void *referent);
void __tm_write_barrier_root(void *referent);
void __tm_write_barrier_mark(void *referent);
void __tm_write_barrier_sweep(void *referent);

void __tm_write_barrier_pure_ignore(void *referent);
void __tm_write_barrier_pure_root(void *referent);
void __tm_write_barrier_pure_mark(void *referent);
void __tm_write_barrier_pure_sweep(void *referent);



#endif
