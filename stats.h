#ifndef tm_STATS_H
#define tm_STATS_H


void tm_validate_lists();
void tm_print_utilization(const char *name, tm_type *t, size_t *n, int nn, size_t *sum);
void tm_print_stats();
void tm_print_block_stats();

#endif
