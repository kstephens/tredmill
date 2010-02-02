#include "tm_data.h"


/*! Global data. */
struct tm_data tm;


/***************************************************************************/
/*! \defgroup internal Internal */
/*@{*/

/**
 * Clears some words on the stack to prevent some garabage.
 *
 * Avoid leaving garbage on the stack
 * Note: Do not move this in to user.c, it might be optimized away.
 */
void __tm_clear_some_stack_words()
{
  int some_words[64];
  memset(some_words, 0, sizeof(some_words));
}


/*@}*/
