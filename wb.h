#ifndef tm_WB_H
#define tm_WB_H

#include <stddef.h>

/* $Id: wb.h,v 1.1 2000-01-07 09:38:32 stephensk Exp $ */

/* A virtual-memory based write-barrier */

typedef void (*tm_wb_fault_handler_t)(void *addr, void *page_addr);
extern tm_wb_fault_handler_t tm_wb_fault_handler;

void tm_wb_protect(void *addr, size_t len);
void tm_wb_unprotect(void *addr, size_t len);

#endif
