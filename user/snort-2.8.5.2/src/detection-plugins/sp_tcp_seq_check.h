/*
** Copyright (C) 2002-2009 Sourcefire, Inc.
** Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
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
*/

/* $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/snort-2.8.5.2/src/detection-plugins/sp_tcp_seq_check.h#1 $ */
#ifndef __SP_TCP_SEQ_CHECK_H__
#define __SP_TCP_SEQ_CHECK_H__

void SetupTcpSeqCheck(void);
uint32_t TcpSeqCheckHash(void *d);
int TcpSeqCheckCompare(void *l, void *r);

#endif  /* __SP_TCP_SEQ_CHECK_H__ */
