/*
** $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/snort-2.8.5.2/src/sfutil/bitop.h#1 $
** 
** bitopt.c
**
** Copyright (C) 2002-2009 Sourcefire, Inc.
** Dan Roelker <droelker@sourcefire.com>
** Marc Norton <mnorton@sourcefire.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** NOTES
**   5.15.02 - Initial Source Code. Norton/Roelker
**   5.23.02 - Moved bitop functions to bitop.h to inline. Norton/Roelker
**   1.21.04 - Added static initialization. Roelker
**   9.13.05 - Separated type and inline func definitions. Sturges
** 
*/

#ifndef _BITOP_H
#define _BITOP_H

typedef struct _BITOP {
    unsigned char *pucBitBuffer;
    unsigned int  uiBitBufferSize;
    unsigned int  uiMaxBits;
} BITOP;

#endif /* _BITOP_H */
