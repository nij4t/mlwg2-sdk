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
 *  $Id: sntp.h,v 1.2 2003/04/17 01:14:04 fuyuki Exp $
 */

#ifndef _SNTP_H
#define _SNTP_H

#include <sys/time.h>

struct sntp {
  int li;
  int vn;
  int mode;
  int stratum;
  int poll;
  signed char precision;
  double delay;
  double dispersion;
  unsigned char identifier[4];
  double reference;
  double originate;
  double receive;
  double transmit;
};

#define SNTP_HEADER_SIZE 48

void sntp_pack(unsigned char *, const struct sntp *);
void sntp_unpack(struct sntp *, const unsigned char *);
void sntp_tstotv(double, struct timeval *);
double sntp_tvtots(const struct timeval *);
double sntp_now();
const char *sntp_inspect(const struct sntp *);

#endif
