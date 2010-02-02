/** \file debug.h
 * \brief Debug support.
 */
#ifndef tm_DEBUG_H
#define tm_DEBUG_H

#include <stdio.h>

/****************************************************************************/
/* Names */

extern const char *tm_struct_name[];

/****************************************************************************/
/* Debug messages. */

extern FILE *tm_msg_file;
extern const char *tm_msg_prefix;
extern const char *tm_msg_enable_default;
extern int tm_msg_enable_all;

void tm_msg_init();
void tm_msg_enable(const char *codes, int enable);

extern int _tm_msg_ignored;
void tm_msg(const char *format, ...);
void tm_msg1(const char *format, ...);

void tm_fatal();
void tm_abort();
void tm_stop();

void _tm_assert(const char *expr, const char *file, int lineno);

#ifndef tm_ASSERT
#define tm_ASSERT 1
#endif

#if tm_ASSERT
#define tm_assert(X,Y...)do { if ( ! (X) ) {_tm_assert(#X, __FILE__, __LINE__); tm_msg1("" Y); tm_msg1("\n"); tm_abort();} } while(0)

#define tm_assert_test(X,Y...)do { if ( ! (X) ) {_tm_assert(#X, __FILE__, __LINE__); tm_msg1("" Y); tm_msg1("\n"); tm_abort();} } while(0)
#define tm_assert_equal(X,Y,F)do { if ( (X) != (Y) ) {_tm_assert(#X " == " #Y, __FILE__, __LINE__); tm_msg1(#X " == " F, X); tm_msg1(", " #Y " == " F, Y); tm_msg1("\n"); tm_abort(); } } while(0)
#else
#define tm_assert(X,Y...)(void)0
#define tm_assert_test(X,Y...)(void)0
#define tm_assert_equal(X,Y,F)(void)0
#endif

#define tm_warn(X,Y...) ((X) ? 0 : (tm_msg1("\n"), tm_msg("WARNING assertion \"%s\" failed %s:%d ", #X, __FILE__, __LINE__), tm_msg1("" Y), tm_msg1("\n"), tm_stop(), 1))


/***************************************************************************/
/* Validation */

void tm_validate_lists();

extern int _tm_sweep_is_error;
int _tm_check_sweep_error();

#endif

