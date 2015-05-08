/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
**
** Last modified: March 4, 2015
**
** License: 
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _SEEQLIB_H_
#define _SEEQLIB_H_

#define LIBSEEQ_VERSION "libseeq-1.0"
#define COLOR_TERMINAL 1

// Match options.
#define SQ_ANY        0x00
#define SQ_MATCH      0x01
#define SQ_NOMATCH    0x02
#define SQ_COUNTLINES 0x04
#define SQ_COUNTMATCH 0x08
#define SQ_BEST       0x10
#define SQ_FIRST      0x00
#define SQ_CONT       0x20
#define SQ_SKIP       0x00

#include <stdio.h>

extern int seeqerr;

typedef struct seeq_t     seeq_t;
typedef struct match_t    match_t;
typedef struct seeqfile_t seeqfile_t;

struct match_t {
   size_t   start;
   size_t   end;
   size_t   line;
   int      dist;
   char   * string;
};

struct seeq_t {
   match_t match;
   int     tau;
   int     wlen;
   char  * keys;
   char  * rkeys;
   void  * dfa;
   void  * rdfa;
};

struct seeqfile_t {
   size_t  line;
   FILE  * fdi;
};

seeq_t     * seeqNew         (const char *, int);
void         seeqFree        (seeq_t *);
seeqfile_t * seeqOpen        (const char *);
int          seeqClose       (seeqfile_t *);
long         seeqFileMatch   (seeqfile_t *, seeq_t *, int);
long         seeqStringMatch (const char *, seeq_t *, int);
size_t       seeqGetLine     (seeq_t *);
size_t       seeqGetStart    (seeq_t *);
size_t       seeqGetEnd      (seeq_t *);
int          seeqGetDistance (seeq_t *);
char       * seeqGetString   (seeq_t *);
const char * seeqPrintError  (void);

#define RESET       "\033[0m"
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */

#endif
