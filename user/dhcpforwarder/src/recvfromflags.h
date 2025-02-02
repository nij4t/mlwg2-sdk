// $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/dhcpforwarder/src/recvfromflags.h#1 $    --*- c++ -*--

// Copyright (C) 2002 Enrico Scholz <enrico.scholz@informatik.tu-chemnitz.de>
//  
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
//  
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//  
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//  

#ifndef H_DHCP_FORWARDER_SRC_RECVFROMFLAGS_H
#define H_DHCP_FORWARDER_SRC_RECVFROMFLAGS_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

ssize_t
recvfrom_flags(int fd,
	       /*@out@*//*@dependent@*/void *ptr, size_t nbytes,
	       int *flagsp,
	       /*@out@*/struct sockaddr *sa, socklen_t *salenptr,
	       /*@out@*/struct in_pktinfo *pktp)
  /*:requires maxSet(ptr) >= nbytes@*/
  /*@globals errno, internalState@*/
  /*@modifies errno, internalState, *ptr, *flagsp, *sa, *salenptr, *pktp@*/
  ;


#endif	/* H_DHCP_FORWARDER_SRC_RECVFROMFLAGS_H */

  // Local Variables:
  // compile-command: "make -C .. -k"
  // fill-column: 80
  // End:
