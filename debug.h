#ifndef tm_DEBUG_H
#define tm_DEBUG_H

#include <stdio.h>

/****************************************************************************/
/* Names */

extern const char *tm_color_name[];
extern const char *tm_struct_name[];
extern const char *tm_phase_name[];

/****************************************************************************/
/* Debug messages. */

extern FILE *tm_msg_file;
extern const char *tm_msg_prefix;
extern const char *tm_msg_enable_default;
extern int tm_msg_enable_all;

void tm_msg_enable(const char *codes, int enable);

extern int _tm_msg_ignored;
void tm_msg(const char *format, ...);
void tm_msg1(const char *format, ...);

void tm_fatal();
void tm_abort();
void tm_stop();

void _tm_assert(const char *expr, const char *file, int lineno);

#define tm_assert(X,Y...)do { if ( ! (X) ) {_tm_assert(#X, __FILE__, __LINE__); tm_msg1("" Y), tm_msg1("\n"), tm_abort();} } while(0)

#if 1
#define tm_assert_test(X,Y...)do { if ( ! (X) ) {_tm_assert(#X, __FILE__, __LINE__); tm_msg1("" Y), tm_msg1("\n"), tm_abort();} } while(0)
#else
#define tm_assert_test(X,Y...)(void)0
#endif

#define tm_warn(X,Y...) ((X) ? 0 : (tm_msg1("\n"), tm_msg("WARNING assertion \"%s\" failed %s:%d ", #X, __FILE__, __LINE__), tm_msg1("" Y), tm_msg1("\n"), tm_stop(), 1))


/***************************************************************************/
/* Validation */

void tm_validate_lists();

extern int _tm_sweep_is_error;
int _tm_check_sweep_error();

#endif
