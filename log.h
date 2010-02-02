#ifndef _tm_LOG_H
#define _tm_LOG_H

#include <stdio.h>

extern
const char *tm_alloc_log_name;

extern
FILE *tm_alloc_log_fh;

void tm_alloc_log_init();
void tm_alloc_log(void *ptr);

#endif
