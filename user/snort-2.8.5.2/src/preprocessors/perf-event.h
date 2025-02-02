/*
**  $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/snort-2.8.5.2/src/preprocessors/perf-event.h#1 $
**
**  perf-event.h
**
**  Copyright (C) 2002-2009 Sourcefire, Inc.
**  Marc Norton <mnorton@sourcefire.com>
**  Dan Roelker <droelker@sourcefire.com>
**
**  NOTES
**  5.28.02 - Initial Source Code. Norton/Roelker
**
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License Version 2 as
**  published by the Free Software Foundation.  You may not use, modify or
**  distribute this program under any other version of the GNU General
**  Public License.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
*/

#ifndef __PERF_EVENT__
#define __PERF_EVENT__

#include "sf_types.h"

typedef struct _SFEVENT {

    uint64_t NQEvents;
    uint64_t QEvents;

    uint64_t TotalEvents;

} SFEVENT;

typedef struct _SFEVENT_STATS {

    uint64_t NQEvents;
    uint64_t QEvents;

    uint64_t TotalEvents;

    double NQPercent;
    double QPercent;

}  SFEVENT_STATS;

/*
**  These functions are for interfacing with the main
**  perf module.
*/ 
int InitEventStats(SFEVENT *sfEvent);
int ProcessEventStats(SFEVENT *sfEvent);

/*
**  These functions are external for updating the
**  SFEVENT structure.
*/
int UpdateNQEvents(SFEVENT *);
int UpdateQEvents(SFEVENT *);

#endif
