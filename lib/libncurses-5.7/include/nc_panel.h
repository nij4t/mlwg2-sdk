/****************************************************************************
 * Copyright (c) 1998-2000,2008 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Juergen Pfeifer                        1997                     *
 *     and: Thomas E. Dickey                                                *
 ****************************************************************************/


/*
 * $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/lib/libncurses-5.7/include/nc_panel.h#1 $
 *
 *	nc_panel.h
 *
 *	Headerfile to provide an interface for the panel layer into
 *      the SCREEN structure of the ncurses core.
 */

#ifndef NC_PANEL_H
#define NC_PANEL_H 1

#ifdef __cplusplus
extern "C" {
#endif

struct panel; /* Forward Declaration */

struct panelhook {
  struct panel*   top_panel;
  struct panel*   bottom_panel;
  struct panel*   stdscr_pseudo_panel;
#if NO_LEAKS
  int (*destroy)(struct panel *);
#endif
};

/* Retrieve the panelhook of the current screen */
extern NCURSES_EXPORT(struct panelhook*) _nc_panelhook (void);

#ifdef __cplusplus
}
#endif

#endif /* NC_PANEL_H */
