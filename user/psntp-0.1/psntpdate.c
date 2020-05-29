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
 *  $Id: psntpdate.c,v 1.4 2003/04/18 00:42:41 fuyuki Exp $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "../gt_log/log.h"
#include "sntp.h"

//lib /*hebe added*/
#include "../nvram/nvram_lib.h"

const char *program_name = "psntpdate";
const double max_offset_adjtime = 1.0;
const double max_offset_settimeofday = 10.0;

enum action { SHOW, ADJTIME, SETTIMEOFDAY };
enum action action = SHOW;
struct timeval timeout;
int debug = 0;
int force = 0;

void usage()
{
  fprintf(stderr, "usage: %s [-46afr] [-t timeout] server\n", program_name);
  exit(1);
}

void print_time(double ts)
{
  struct timeval tv;
  time_t time;
  struct tm *tm;
  char buf[64];
  const char *fmt = "%a %b %d %H:%M:%S %Z %Y";

  sntp_tstotv(ts, &tv);
  time = tv.tv_sec;
  tm = localtime(&time);
  if (strftime(buf, sizeof buf, fmt, tm) > 0)
    printf(buf);
}

int query(int s, const struct addrinfo *res, struct sntp *sntp)
{
  unsigned char buf[SNTP_HEADER_SIZE];
  struct sockaddr_storage ss;
  socklen_t size = sizeof ss;

  sntp->li = 0;
  sntp->vn = 4;
  sntp->mode = 3;
  sntp->stratum = 0;
  sntp->poll = 0;
  sntp->precision = 0;
  sntp->delay = 0.0;
  sntp->dispersion = 0.0;
  memset(sntp->identifier, 0, sizeof sntp->identifier);
  sntp->reference = 0.0;
  sntp->originate = 0.0;
  sntp->receive = 0.0;
  sntp->transmit = sntp_now();

  sntp_pack(buf, sntp);

  if (sendto(s, buf, sizeof buf, 0, res->ai_addr, res->ai_addrlen) < 0)
    return -1;

  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0)
    return -1;

  if (recvfrom(s, buf, sizeof buf, 0, (struct sockaddr *) &ss, &size) < 0)
    return -1;

  sntp_unpack(sntp, buf);

  if (debug)
    printf(sntp_inspect(sntp));

  return 0;
}

int do_action(const struct sntp *sntp, enum action action)
{
  double t1 = sntp->originate;
  double t2 = sntp->receive;
  double t3 = sntp->transmit;
  double t4 = sntp_now();
  double d = (t4 - t1) - (t2 - t3);
  double t = ((t2 - t1) + (t3 - t4)) / 2.0;
  int r;
	time_t up;
	char uptime[32];
  
  if (debug)
    printf("delay %f, offset %f\n", d, t);

  switch (action) {
  case ADJTIME:
    if (fabs(t) > max_offset_adjtime) {
      fprintf(stderr, "%s: Offset range exceeded: %f\n", program_name, t);
      return 1;
    }
    break;
  case SETTIMEOFDAY:
    if (!force && fabs(t) > max_offset_settimeofday) {
      fprintf(stderr, "%s: Offset range exceeded: %f\n", program_name, t);
      return 1;
    }
    break;
  default:
    break;
  }

  r = 0;
  switch (action) {
  case SHOW:
    print_time(t4 + t);
    printf("\n");
    break;
  case ADJTIME:
    {
      double i, f;
      struct timeval delta;
      f = modf(t, &i);
      delta.tv_sec = i;
      delta.tv_usec = f * 1e6;
      r = adjtime(&delta, NULL);
      if (r == 0) {
      	printf("adjusting %+.3f seconds\n", t);
      }
    }
    break;
  case SETTIMEOFDAY:
    {
      struct timeval now;
      sntp_tstotv(t4 + t, &now);
      r = settimeofday(&now, NULL);

      if (r == 0) {
		printf("\n\n=== psntpdate: checkntp_gettime=1 ===\n\n");
		fflush(NULL);
		nvram_set("checkntp_gettime", "1");
		if(!strcmp("1", nvram_safe_get("set_date"))){
			nvram_set("set_date", "0");
			nvram_set("reset_eco", "1");
		}
		nvram_commit();
		up = time(NULL);
		sprintf(uptime,"%ld",up);
		nvram_set("UPnPWANUpTime",uptime);
		
		
        print_time(t4);
        printf(" -> ");
        print_time(t4 + t);
        printf("\n");
        char log[128];
        setLogLevel(_LOGINFO, "NTP"); // Set Log level to debug
        sprintf(log, "adjusting %+.3f seconds\n", t);
        addLog(_LOGINFO, log); // log
      }
	  else{
        printf("\n\n=== psntpdate: checkntp_gettime=0  ===\n\n");
		fflush(NULL);
		nvram_set("checkntp_gettime", "0");
		nvram_commit();
	  }
    }
    break;
  }

  return r;
}

int main(int argc, char *argv[])
{
  int c, r, s;
  struct addrinfo hints, *res;
  struct sntp sntp;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = PF_UNSPEC;

  timeout.tv_sec = 5;
  timeout.tv_usec = 0;

  while ((c = getopt(argc, argv, "46adfrt:")) != -1) {
    switch (c) {
    case '4':
      hints.ai_family = AF_INET;
      break;
    case '6':
      hints.ai_family = AF_INET6;
      break;
    case 'a':
      action = ADJTIME;
      break;
    case 'd':
      debug = 1;
      break;
    case 'f':
      force = 1;
      break;
    case 'r':
      action = SETTIMEOFDAY;
      break;
    case 't':
      timeout.tv_sec = strtol(optarg, NULL, 10);
      break;
    default:
      usage();
      break;
    }
  }
  argc -= optind;
  argv += optind;

  if (argc < 1)
    usage();

 hints.ai_family = AF_INET;//Fixed to IPv4 avoid socket not support IPv6.
  hints.ai_socktype = SOCK_DGRAM;
  r = getaddrinfo(argv[0], "ntp", &hints, &res);
  if (r != 0) {
    fprintf(stderr, "%s: %s\n", program_name, gai_strerror(r));
    exit(1);
  }

  s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (s < 0 || query(s, res, &sntp) < 0) {
    perror(program_name);
    exit(1);
  }

  freeaddrinfo(res);

  if (do_action(&sntp, action) < 0) {
    perror(program_name);
    exit(1);
  }

  return 0;
}
