/* Copyright (C) 1996-2021 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

// Obtained from glibc-2.34
// With minor modifications to improve portability, as well as allow ipv4
//   addresses with 0-prefixed (max 3-digit) octets.
// This also adds an AF_UNSPEC case which tries both address types.
#include "inet_pton.h"

#include <string.h>

// resolv/arpa/nameser.h
#define NS_INT16SZ	2	/*%< #/bytes of data in a uint16_t */
#define NS_INADDRSZ	4	/*%< IPv4 T_A */
#define NS_IN6ADDRSZ	16	/*%< IPv6 T_AAAA */

// bits/socket.h
#define	PF_UNSPEC	0	/* Unspecified.  */
#define	PF_INET		2	/* IP protocol family.  */
#define PF_INET6	26	/* IP version 6.  */
#define	AF_UNSPEC	PF_UNSPEC
#define	AF_INET		PF_INET
#define AF_INET6	PF_INET6

static int inet_pton4 (const char *src, const char *src_end, unsigned char *dst);
static int inet_pton6 (const char *src, const char *src_end, unsigned char *dst);

int
mobile_pton_length (int af, const char *src, size_t srclen, void *dst)
{
  switch (af)
    {
    case AF_INET:
      return inet_pton4 (src, src + srclen, dst);
    case AF_INET6:
      return inet_pton6 (src, src + srclen, dst);
    case AF_UNSPEC:
      if (inet_pton4 (src, src + srclen, dst))
        return AF_INET;
      if (inet_pton6 (src, src + srclen, dst))
        return AF_INET6;
      return -1;
    default:
      return -1;
    }
}

/* Like inet_pton_length, but use strlen (SRC) as the length of
   SRC.  */
int
mobile_pton (int af, const char *src, void *dst)
{
  return mobile_pton_length (af, src, strlen (src), dst);
}

/* Like inet_aton but without all the hexadecimal, octal and shorthand
   (and trailing garbage is not ignored).  Return 1 if SRC is a valid
   dotted quad, else 0.  This function does not touch DST unless it's
   returning 1.
   Author: Paul Vixie, 1996.  */
static int
inet_pton4 (const char *src, const char *end, unsigned char *dst)
{
  int saw_digit, octets, ch;
  unsigned char tmp[NS_INADDRSZ], *tp;

  saw_digit = 0;
  octets = 0;
  *(tp = tmp) = 0;
  while (src < end)
    {
      ch = *src++;
      if (ch >= '0' && ch <= '9')
        {
          unsigned int new = *tp * 10 + (ch - '0');

          if (saw_digit > 3)
            return 0;
          if (new > 255)
            return 0;
          *tp = new;
          if (! saw_digit++)
            {
              if (++octets > 4)
                return 0;
            }
        }
      else if (ch == '.' && saw_digit)
        {
          if (octets == 4)
            return 0;
          *++tp = 0;
          saw_digit = 0;
        }
      else
        return 0;
    }
  if (octets < 4)
    return 0;
  memcpy (dst, tmp, NS_INADDRSZ);
  return 1;
}

/* Return the value of CH as a hexademical digit, or -1 if it is a
   different type of character.  */
static int
hex_digit_value (char ch)
{
  if ('0' <= ch && ch <= '9')
    return ch - '0';
  if ('a' <= ch && ch <= 'f')
    return ch - 'a' + 10;
  if ('A' <= ch && ch <= 'F')
    return ch - 'A' + 10;
  return -1;
}

/* Convert presentation-level IPv6 address to network order binary
   form.  Return 1 if SRC is a valid [RFC1884 2.2] address, else 0.
   This function does not touch DST unless it's returning 1.
   Author: Paul Vixie, 1996.  Inspired by Mark Andrews.  */
static int
inet_pton6 (const char *src, const char *src_endp, unsigned char *dst)
{
  unsigned char tmp[NS_IN6ADDRSZ], *tp, *endp, *colonp;
  const char *curtok;
  int ch;
  size_t xdigits_seen;	/* Number of hex digits since colon.  */
  unsigned int val;

  tp = memset (tmp, '\0', NS_IN6ADDRSZ);
  endp = tp + NS_IN6ADDRSZ;
  colonp = NULL;

  /* Leading :: requires some special handling.  */
  if (src == src_endp)
    return 0;
  if (*src == ':')
    {
      ++src;
      if (src == src_endp || *src != ':')
        return 0;
    }

  curtok = src;
  xdigits_seen = 0;
  val = 0;
  while (src < src_endp)
    {
      ch = *src++;
      int digit = hex_digit_value (ch);
      if (digit >= 0)
	{
	  if (xdigits_seen == 4)
	    return 0;
	  val <<= 4;
	  val |= digit;
	  if (val > 0xffff)
	    return 0;
	  ++xdigits_seen;
	  continue;
	}
      if (ch == ':')
	{
	  curtok = src;
	  if (xdigits_seen == 0)
	    {
	      if (colonp)
		return 0;
	      colonp = tp;
	      continue;
	    }
	  else if (src == src_endp)
            return 0;
	  if (tp + NS_INT16SZ > endp)
	    return 0;
	  *tp++ = (unsigned char) (val >> 8) & 0xff;
	  *tp++ = (unsigned char) val & 0xff;
	  xdigits_seen = 0;
	  val = 0;
	  continue;
	}
      if (ch == '.' && ((tp + NS_INADDRSZ) <= endp)
          && inet_pton4 (curtok, src_endp, tp) > 0)
	{
	  tp += NS_INADDRSZ;
	  xdigits_seen = 0;
	  break;  /* '\0' was seen by inet_pton4.  */
	}
      return 0;
    }
  if (xdigits_seen > 0)
    {
      if (tp + NS_INT16SZ > endp)
	return 0;
      *tp++ = (unsigned char) (val >> 8) & 0xff;
      *tp++ = (unsigned char) val & 0xff;
    }
  if (colonp != NULL)
    {
      /* Replace :: with zeros.  */
      if (tp == endp)
        /* :: would expand to a zero-width field.  */
        return 0;
      size_t n = tp - colonp;
      memmove (endp - n, colonp, n);
      memset (colonp, 0, endp - n - colonp);
      tp = endp;
    }
  if (tp != endp)
    return 0;
  memcpy (dst, tmp, NS_IN6ADDRSZ);
  return 1;
}
