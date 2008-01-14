/* $Id: wb.c,v 1.3 2008-01-14 00:08:02 stephens Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <signal.h>
#include <assert.h>
#include "internal.h"
#include "util/bitset.h"
#include "wb.h"

/*********************************************************************/
/* Configuration */

/*
#define MAX_ADDR_SPACE tm_PTR_RANGE
*/

#ifndef MAX_ADDR_SPACE
#define MAX_ADDR_SPACE (512 * 1024 * 1024) /* 512Mb */
#endif

/*********************************************************************/
/* Page mutated bits */

/* mutated bits indexed by page no. */
bitset_t tm_wb_page_mutated_bits[bitset_ELEM_LEN(MAX_ADDR_SPACE / tm_PAGESIZE)];

#define tm_wb_ptr_to_page_ptr(ptr) ((void *)(((unsigned long) (ptr)) & ~(tm_PAGESIZE-1)))

#define tm_wb_ptr_to_page_no(ptr) ((unsigned long) (ptr) / tm_PAGESIZE)

#define tm_wb_ptr_mutated_get(ptr) bitset_get(tm_wb_page_mutated_bits, tm_wb_ptr_to_page_no(ptr))
#define tm_wb_ptr_mutated_set(ptr) bitset_set(tm_wb_page_mutated_bits, tm_wb_ptr_to_page_no(ptr))
#define tm_wb_ptr_mutated_clr(ptr) bitset_clr(tm_wb_page_mutated_bits, tm_wb_ptr_to_page_no(ptr))



#if 0
typedef struct tm_wb_block {
  struct tm_wb_block *next, *prev;
  char *begin;
  size_t len;
} tm_wb_block;

#endif

/*********************************************************************/
/* Write fault handler */

static
void tm_wb_unprotect_page(void *page_addr)
{
  if ( mprotect(page_addr, tm_PAGESIZE, PROT_READ | PROT_WRITE) ) {
    perror("unprotect: cannot undo write protect");
    abort();
  }  
}


/***********************************************************************/
/* write fault handler */

static void tm_wb_fault_default(void *addr, void *page_addr)
{
#if 1
  fprintf(stderr, "tm_wb: WRITE FAULT: %p: page %p\n", 
	  (void*) addr, 
	  (void*) page_addr
	  );
#endif  
}

tm_wb_fault_handler_t tm_wb_fault_handler = tm_wb_fault_default;

/***********************************************************************/
/* write fault signal handler */

#ifdef linux
static void tm_wb_signal(int sig, struct sigcontext sc)
#endif

#if 0
static void tm_wb_signal(int sig, struct siginfo *si, void *vp)
#endif

{
  char *addr, *page_addr;

  /* Get the address of the write fault. */
#ifdef linux
  addr = (char*) sc.cr2;
#endif

  /* Get the page-aligned address */
  page_addr = tm_wb_ptr_to_page_ptr(addr);

  /* Mark the page as mutated. */
  tm_wb_ptr_mutated_set(addr);

  /* Stop write faulting for this page. */
  tm_wb_unprotect_page(page_addr);

  /* Call the fault handler. */
  tm_wb_fault_handler(addr, page_addr);

  /* Return to mutator. */
}

static int tm_wb_inited = 0;
static void tm_wb_init()
{
  tm_wb_inited ++;

  /* Set signal handler to trap write faults. */
  {
    struct sigaction sa, sa_old;

    sa.sa_sigaction = (void*) tm_wb_signal;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if ( sigaction(SIGSEGV, &sa, &sa_old) ) {
      perror("sigaction");
      abort();
    }
  }  
}

/************************************************************************/
/* User level routines. */

void tm_wb_protect(void *addr, size_t len)
{
  if ( ! tm_wb_inited ) tm_wb_init();

  assert(len < tm_PAGESIZE);

  addr = tm_wb_ptr_to_page_ptr(addr);

  tm_wb_ptr_mutated_clr(addr);

  if ( mprotect(addr, tm_PAGESIZE, PROT_READ) ) {
    perror("mprotect: cannot write protect");
    abort();
  }  
}

void tm_wb_unprotect(void *addr, size_t len)
{
  if ( ! tm_wb_inited ) tm_wb_init();

  assert(len < tm_PAGESIZE);

  addr = tm_wb_ptr_to_page_ptr(addr);

  tm_wb_unprotect_page(addr);
}

/************************************************************************/
/* Unit test. */

#define tm_UNIT_TEST
#ifdef tm_UNIT_TEST
int main(int argc, char **argv)
{
  char *p;
  size_t plen;
  int i;
  char *x;
  size_t xi;

  plen = 256;
  p = malloc(plen);

  for ( i = 0; i < plen; i ++ ) {
    p[i] = i;
  }

  tm_wb_protect(p, plen);

  fprintf(stderr, "p = %p[%d]\n", p, plen);

  xi = 12;
  x = p + xi;

  fprintf(stderr, "mutated(%p) = %d\n", x, tm_wb_ptr_mutated_get(x) ? 1 : 0);
  fprintf(stderr, "%p[%d] = %lu\n", p, xi, * (unsigned long*) x);

  /* Cause write fault. */
  *(long *) x = 12345;

  fprintf(stderr, "mutated(%p) = %d\n", x, tm_wb_ptr_mutated_get(x) ? 1 : 0);
  fprintf(stderr, "%p[%d] = %lu\n", p, xi, * (unsigned long*) x);

  /* Doesn't cause write fault. */
  *(long *) x = 54321;

  fprintf(stderr, "mutated(%p) = %d\n", x, tm_wb_ptr_mutated_get(x) ? 1 : 0);
  fprintf(stderr, "%p[%d] = %lu\n", p, xi, * (unsigned long*) x);

  return 0;
}

#endif
