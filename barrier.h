/** \file barrier.h
 * \brief Write barriers.
 */
#ifndef tm_BARRIER_H
#define tm_BARRIER_H

void __tm_write_barrier_root_ignore(void *referent);
void __tm_write_barrier_root_ROOT(void *referent);
void __tm_write_barrier_root_SCAN(void *referent);
void __tm_write_barrier_root_SWEEP(void *referent);

void __tm_write_barrier_ignore(void *referent);
void __tm_write_barrier_ROOT(void *referent);
void __tm_write_barrier_SCAN(void *referent);
void __tm_write_barrier_SWEEP(void *referent);

void __tm_write_barrier_pure_ignore(void *referent);
void __tm_write_barrier_pure_ROOT(void *referent);
void __tm_write_barrier_pure_SCAN(void *referent);
void __tm_write_barrier_pure_SWEEP(void *referent);



#endif
