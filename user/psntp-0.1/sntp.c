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
 *  $Id: sntp.c,v 1.1.1.1 2003/04/16 01:42:07 fuyuki Exp $
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include "sntp.h"

#define DIFF19901970 2208988800.0

void pack_ul(unsigned char *buf, unsigned long val)
{
  buf[0] = (val >> 24) & 0xff;
  buf[1] = (val >> 16) & 0xff;
  buf[2] = (val >> 8) & 0xff;
  buf[3] = val & 0xff;
}

unsigned long unpack_ul(const unsigned char *buf)
{
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

void pack_ts(unsigned char *buf, double ts)
{
  double i, f;

  f = modf(ts, &i);
  pack_ul(buf, (unsigned long) i);
  pack_ul(buf + 4, (unsigned long) (f * 4294967296.0));
}

double unpack_ts(const unsigned char *buf)
{
  return unpack_ul(buf) + unpack_ul(buf + 4) / 4294967296.0;
}

void sntp_pack(unsigned char *buf, const struct sntp *sntp)
{
  buf[0] = (sntp->li << 6) | (sntp->vn << 3) | sntp->mode;
  buf[1] = sntp->stratum;
  buf[2] = sntp->poll;
  buf[3] = (signed char) sntp->precision;
  pack_ul(&buf[4], sntp->delay * 65536.0);
  pack_ul(&buf[8], sntp->dispersion * 65536.0);
  memcpy(&buf[12], &sntp->identifier, 4);
  pack_ts(&buf[16], sntp->reference);
  pack_ts(&buf[24], sntp->originate);
  pack_ts(&buf[32], sntp->receive);
  pack_ts(&buf[40], sntp->transmit);
}

void sntp_unpack(struct sntp *sntp, const unsigned char *buf)
{
  sntp->li = buf[0] >> 6;
  sntp->vn = (buf[0] >> 3) & 0x07;
  sntp->mode = buf[0] & 0x07;
  sntp->stratum = buf[1];
  sntp->poll = buf[2];
  sntp->precision = buf[3];
  sntp->delay = unpack_ul(&buf[4]) / 65536.0;
  sntp->dispersion = unpack_ul(&buf[8]) / 65536.0;
  memcpy(&sntp->identifier, &buf[12], 4);
  sntp->reference = unpack_ts(&buf[16]);
  sntp->originate = unpack_ts(&buf[24]);
  sntp->receive = unpack_ts(&buf[32]);
  sntp->transmit = unpack_ts(&buf[40]);
}

void sntp_tstotv(double ts, struct timeval *tv)
{
  double i, f;

  /* don't care the leap seconds */
  f = modf(ts - DIFF19901970, &i);
  tv->tv_sec = i;
  tv->tv_usec = f * 1e6;
}

double sntp_tvtots(const struct timeval *tv)
{
  /* don't care the leap seconds */
  return DIFF19901970 + tv->tv_sec + tv->tv_usec * 1e-6;
}

double sntp_now()
{
  struct timeval now;

  if (gettimeofday(&now, NULL) < 0)
    return -1;

  return sntp_tvtots(&now);
}

const char *sntp_inspect(const struct sntp *sntp)
{
  static char buf[1024];

  snprintf(buf, sizeof buf,
	   "li: %d\n"
	   "vn: %d\n"
	   "mode:%d\n"
	   "stratum: %d\n"
	   "poll: %d\n"
	   "precision: %d\n"
	   "delay: %f\n"
	   "dispersion: %f\n"
	   "identifier: %02x %02x %02x %02x\n"
	   "reference: %f\n"
	   "originate: %f\n"
	   "receive: %f\n"
	   "transmit: %f\n",
	   sntp->li, sntp->vn, sntp->mode,
	   sntp->stratum, sntp->poll, sntp->precision,
	   sntp->delay, sntp->dispersion,
	   sntp->identifier[0], sntp->identifier[1],
	   sntp->identifier[2], sntp->identifier[3],
	   sntp->reference, sntp->originate, sntp->receive, sntp->transmit);

  return buf;
}
