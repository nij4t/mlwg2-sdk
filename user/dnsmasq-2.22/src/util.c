/* dnsmasq is Copyright (c) 2000 - 2005 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/


/* Some code in this file contributed by Rob Funk. */

#include "dnsmasq.h"

/* Prefer arc4random(3) over random(3) over rand(3) */
/* Also prefer /dev/urandom over /dev/random, to preserve the entropy pool */
#ifdef HAVE_ARC4RANDOM
# define rand()		arc4random()
# define srand(s)	(void)0
# define RANDFILE	(NULL)
#else
# ifdef HAVE_RANDOM
#  define rand()	random()
#  define srand(s)	srandom(s)
# endif
# ifdef HAVE_DEV_URANDOM
#  define RANDFILE	"/dev/urandom"
# else
#  ifdef HAVE_DEV_RANDOM
#   define RANDFILE	"/dev/random"
#  else
#   define RANDFILE	(NULL)
#  endif
# endif
#endif

unsigned short rand16(void)
{
  static int been_seeded = 0;
  const char *randfile = RANDFILE;
  
  if (! been_seeded) 
    {
      int fd, n = 0;
      unsigned int c = 0, seed = 0, badseed;
      char sbuf[sizeof(seed)];
      char *s;
      struct timeval now;

      /* get the bad seed as a backup */
      /* (but we'd rather have something more random) */
      gettimeofday(&now, NULL);
      badseed = now.tv_sec ^ now.tv_usec ^ (getpid() << 16);
      
      fd = open(randfile, O_RDONLY);
      if (fd < 0) 
	seed = badseed;
      else
	{
	  s = (char *) &seed;
	  while ((c < sizeof(seed)) &&
		 ((n = read(fd, sbuf, sizeof(seed)) > 0))) 
	    {
	      memcpy(s, sbuf, n);
	      s += n;
	      c += n;
	    }
	  if (n < 0)
	    seed = badseed;
	  close(fd);
	}

      srand(seed);
      been_seeded = 1;
    }
  
  /* Some rand() implementations have less randomness in low bits
   * than in high bits, so we only pay attention to the high ones.
   * But most implementations don't touch the high bit, so we 
   * ignore that one.
   */
  return( (unsigned short) (rand() >> 15) );
}

int atoi_check(char *a, int *res)
{
  char *p;

  for (p = a; *p; p++)
     if (*p < '0' || *p > '9')
       return 0;

  *res = atoi(a);
  return 1;
}

int legal_char(char c)
{
  /* check for legal char a-z A-Z 0-9 - 
     (also / , used for RFC2317 and _ used in windows queries) */
  if ((c >= 'A' && c <= 'Z') ||
      (c >= 'a' && c <= 'z') ||
      (c >= '0' && c <= '9') ||
      c == '-' || c == '/' || c == '_')
    return 1;
  
  return 0;
}
  
int canonicalise(char *s)
{
  /* check for legal chars and remove trailing . 
     also fail empty string and label > 63 chars */
  size_t dotgap = 0, l = strlen(s);
  char c;

  if (l == 0 || l > MAXDNAME) return 0;

  if (s[l-1] == '.')
    {
      if (l == 1) return 0;
      s[l-1] = 0;
    }
  
  while ((c = *s++))
    if (c == '.')
      dotgap = 0;
    else if (!legal_char(c) || (++dotgap > MAXLABEL))
      return 0;
  
  return 1;
}

/* for use during startup */
void *safe_malloc(size_t size)
{
  void *ret = malloc(size);
  
  if (!ret)
    die("could not get memory", NULL);
     
  return ret;
}
    
char *safe_string_alloc(char *cp)
{
  char *ret = NULL;

  if (cp && strlen(cp) != 0)
    {
      ret = safe_malloc(strlen(cp)+1);
      strcpy(ret, cp);
    }

  return ret;
}

static void log_err(char *message, char *arg1)
{
  char *errmess = strerror(errno);
  
  if (!arg1)
    arg1 = errmess;
  
  fprintf(stderr, "dnsmasq: ");
  fprintf(stderr, message, arg1, errmess);
  fprintf(stderr, "\n");
  
  syslog(LOG_CRIT, message, arg1, errmess);
}

void complain(char *message, int lineno, char *file)
{
  char buff[256];
  
  sprintf(buff, "%s at line %d of %s", message, lineno, file);
  log_err(buff, NULL);
}

void die(char *message, char *arg1)
{
  log_err(message, arg1);
  syslog(LOG_CRIT, "FAILED to start up");
  exit(1);
}

int sockaddr_isequal(union mysockaddr *s1, union mysockaddr *s2)
{
  if (s1->sa.sa_family == s2->sa.sa_family)
    { 
      if (s1->sa.sa_family == AF_INET &&
	  s1->in.sin_port == s2->in.sin_port &&
	  memcmp(&s1->in.sin_addr, &s2->in.sin_addr, sizeof(struct in_addr)) == 0)
	return 1;
#ifdef HAVE_IPV6      
      if (s1->sa.sa_family == AF_INET6 &&
	  s1->in6.sin6_port == s2->in6.sin6_port &&
	  s1->in6.sin6_flowinfo == s2->in6.sin6_flowinfo &&
	  memcmp(&s1->in6.sin6_addr, &s2->in6.sin6_addr, sizeof(struct in6_addr)) == 0)
	return 1;
#endif
    }
  return 0;
}

int sa_len(union mysockaddr *addr)
{
#ifdef HAVE_SOCKADDR_SA_LEN
  return addr->sa.sa_len;
#else
#ifdef HAVE_IPV6
  if (addr->sa.sa_family == AF_INET6)
    return sizeof(addr->in6);
  else
#endif
    return sizeof(addr->in); 
#endif
}

/* don't use strcasecmp and friends here - they may be messed up by LOCALE */
int hostname_isequal(unsigned char *a, unsigned char *b)
{
  unsigned int c1, c2;
  
  do {
    c1 = *a++;
    c2 = *b++;
    
    if (c1 >= 'A' && c1 <= 'Z')
      c1 += 'a' - 'A';
    if (c2 >= 'A' && c2 <= 'Z')
      c2 += 'a' - 'A';
    
    if (c1 != c2)
      return 0;
  } while (c1);
  
  return 1;
}
    
time_t dnsmasq_time(int fd)
{
#ifdef HAVE_BROKEN_RTC
  /* we use uptime as a time-base, rather than epoch time
     because epoch time can break when a machine contacts
     a nameserver and updates it. */
  char buf[30];
  lseek(fd, 0, SEEK_SET);
  read(fd, buf, 30);
  return (time_t)atol(buf);
#else
  fd = 0; /* stop warning */
  return time(NULL);
#endif
}

int is_same_net(struct in_addr a, struct in_addr b, struct in_addr mask)
{
  return (a.s_addr & mask.s_addr) == (b.s_addr & mask.s_addr);
} 

int retry_send(void)
{
   struct timespec waiter;
   if (errno == EAGAIN)
     {
       waiter.tv_sec = 0;
       waiter.tv_nsec = 10000;
       nanosleep(&waiter, NULL);
       return 1;
     }
   
   if (errno == EINTR)
     return 1;

   return 0;
}

void prettyprint_time(char *buf, unsigned int t)
{
  if (t == 0xffffffff)
    sprintf(buf, "infinite");
  else
    {
      unsigned int x, p = 0;
      if ((x = t/3600))
	p += sprintf(&buf[p], "%dh", x);
      if ((x = (t/60)%60))
	p += sprintf(&buf[p], "%dm", x);
      if ((x = t%60))
	p += sprintf(&buf[p], "%ds", x);
    }
}


/* in may equal out, when maxlen may be -1 (No max len). */
int parse_hex(char *in, unsigned char *out, int maxlen, unsigned int *wildcard_mask)
{
  int mask = 0, i = 0;
  char *r;

  while (maxlen == -1 || i < maxlen)
    {
      for (r = in; *r != 0 && *r != ':' && *r != '-'; r++);
      if (*r == 0)
	maxlen = i;
      if (r != in )
	{
	  *r = 0;
	  mask = mask << 1;
	  if (strcmp(in, "*") == 0)
	    mask |= 1;
	  else
	    out[i] = strtol(in, NULL, 16);
	  i++;
	}
      in = r+1;
    }
  
  if (wildcard_mask)
    *wildcard_mask = mask;

  return i;
}
