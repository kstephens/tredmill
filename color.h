/** \file color.h
 * \brief Color.
 *
 * $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $
 */
#ifndef tredmill_COLOR_H
#define tredmill_COLOR_H


/****************************************************************************/
/*! \defgroup color Color */
/*@{*/


/**
 * The color of a node.
 *
 * Values greater than tm_BLACK are used as indicies
 * into accounting arrays.
 */
typedef enum tm_color {
  /*! Node colors */

  /*! Free, in free list. */
  tm_WHITE,
  /*! Allocated. */
  tm_ECRU,
  /*! Marked in use, scheduled for interior scanning. */
  tm_GREY,
  /*! Marked in use, scanned. */
  tm_BLACK,        

  /*! Accounting */

  /*! Total nodes of any color. */
  tm_TOTAL,        
  tm__LAST,

  /*! Type-level Stats. */

  /*! Total blocks in use. */
  tm_B = tm__LAST,
  tm_CAPACITY = tm_B,
  /*! Total nodes in use. */ 
  tm_NU,
  /*! Total bytes in use. */
  tm_b,
  /*! Avg bytes/node. */
  tm_b_NU,
  tm__LAST2,

  /*! Block-level Stats. */

  /*! Blocks currently allocated from OS. */
  tm_B_OS = tm__LAST2,
  /*! Bytes currently allocated from OS. */ 
  tm_b_OS,
  /*! Peak blocks allocated from OS. */
  tm_B_OS_M,
  /*! Peak bytes allocated from OS. */
  tm_b_OS_M,
  tm__LAST3,

  /*! Aliases for internal structure coloring. */

  /*! Color of a free tm_block. */
  tm_FREE_BLOCK = tm_WHITE,
  /*! Color of a live, in-use tm_block. */
  tm_LIVE_BLOCK = tm_ECRU,
  /*! Color of a free tm_type. */
  tm_FREE_TYPE  = tm_GREY,
  /*! Color of a live, in-use tm_type. */
  tm_LIVE_TYPE  = tm_BLACK

} tm_color;


/*@}*/

#endif
