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
  tm_FREE_BLOCK = 0,
  /*! Color of a live, in-use tm_block. */
  tm_LIVE_BLOCK,
  /*! Color of a free tm_type. */
  tm_FREE_TYPE,
  /*! Color of a live, in-use tm_type. */
  tm_LIVE_TYPE,
  tm__LAST4,

} tm_color;


/*! Color names. */
extern const char *tm_color_name[];


/**
 * Keeps track of current color mappings.
 *
 * When the treads are "flipped" colors change.
 *
 * Rather than altering all the actual colors of the all the nodes,
 * remap the colors logically.
 */
typedef struct tm_colors {
  /**
   * Forward color mappings.
   *
   * c[tm_BLACK] maps to the actual tm_list_color() of the nodes.
   */
  tm_color c[tm_TOTAL];

  /**
   * Inverse color mappings.
   *
   * c1[tm_list_color(node)].
   */
  tm_color c1[tm_TOTAL];
} tm_colors;


static __inline
void tm_colors_init(tm_colors *c)
{
  c->c[tm_WHITE] = c->c1[tm_WHITE] = tm_WHITE;
  c->c[tm_ECRU]  = c->c1[tm_ECRU]  = tm_ECRU;
  c->c[tm_GREY]  = c->c1[tm_GREY]  = tm_GREY;
  c->c[tm_BLACK] = c->c1[tm_BLACK] = tm_BLACK;
}

/**
 * Rotate colors: WHITE, ECRU, BLACK = ECRU, BLACK, GREY.
 */
static __inline
void tm_colors_flip(tm_colors *c)
{
  int tmp;
  
  tmp = c->c[tm_WHITE];
  c->c[tm_WHITE] = c->c[tm_ECRU];
  c->c[tm_ECRU]  = c->c[tm_BLACK];
  c->c[tm_BLACK] = c->c[tm_GREY];
  c->c[tm_GREY]  = tmp;

  c->c1[c->c[tm_WHITE]] = tm_WHITE;
  c->c1[c->c[tm_ECRU]]  = tm_ECRU;
  c->c1[c->c[tm_GREY]]  = tm_GREY;
  c->c1[c->c[tm_BLACK]] = tm_BLACK;
}

/*@}*/

#endif
