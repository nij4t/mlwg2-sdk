/*
 * Copyright(C) 2003 by Kimura Fuyuki <fuyuki@hadaly.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 *  $Id: psntpd.c,v 1.2 2003/04/17 06:31:53 fuyuki Exp $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/socket.h>
#include "sntp.h"

static int debug = 0;

int respond(int s)
{
  unsigned char buf[SNTP_HEADER_SIZE];
  struct sntp sntp;
  struct sockaddr_storage ss;
  socklen_t size = sizeof ss;

  if (recvfrom(s, buf, sizeof buf, 0, (struct sockaddr *) &ss, &size) < 0)
    return -1;

  sntp_unpack(&sntp, buf);

  if (debug)
    syslog(LOG_DEBUG, sntp_inspect(&sntp));

  sntp.li = 0;
  sntp.mode = (sntp.mode == 3) ? 4 : 2;
  sntp.stratum = 1;
  sntp.precision = -6;
  sntp.delay = 0.0;
  sntp.dispersion = 0.0;
  strncpy((char *) sntp.identifier, "LOCL", sizeof sntp.identifier);
  sntp.reference = sntp_now();
  sntp.originate = sntp.transmit;
  sntp.receive = sntp.reference;
  sntp.transmit = sntp.reference;

  sntp_pack(buf, &sntp);

  if (sendto(s, buf, sizeof buf, 0, (struct sockaddr *) &ss, size) < 0)
    return -1;

  return 0;
}

int main(int argc, char *argv[])
{
  int c;

  while ((c = getopt(argc, argv, "d")) != -1) {
    switch (c) {
    case 'd':
      debug = 1;
      break;
    default:
      break;
    }
  }
  argc -= optind;
  argv += optind;

  if (respond(0) < 0)
    exit(1);

  return 0;
}
