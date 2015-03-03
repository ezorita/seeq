/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
**
** Last modified: March 3, 2015
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

#define _GNU_SOURCE

#ifndef _SEEQLIB_H_
#define _SEEQLIB_H_

#define LIBSEEQ_VERSION "libseeq-1.0"
#define COLOR_TERMINAL 1

// Match options.
#define SQ_ANY     0x00
#define SQ_MATCH   0x01
#define SQ_NOMATCH 0x02
#define SQ_COUNT   0x04
#define SQ_BEST    0x10
#define SQ_FIRST   0x00

#include <stdio.h>

extern int seeqerr;

typedef struct seeq_t   seeq_t;
typedef struct match_t  match_t;

struct match_t {
   int    start;
   int    end;
   int    dist;
   long   line;
   char * string;
};

struct seeq_t {
   match_t match;
   int     tau;
   int     wlen;
   long    line;
   long    count;
   char  * keys;
   char  * rkeys;
   void  * dfa;
   void  * rdfa;
   FILE  * fdi;
};

seeq_t    * seeqOpen        (char *, char *, int);
int         seeqClose       (seeq_t *);
long        seeqMatch       (seeq_t *, int);
long        seeqGetLine     (seeq_t *);
int         seeqGetDistance (seeq_t *);
int         seeqGetStart    (seeq_t *);
int         seeqGetEnd      (seeq_t *);
char      * seeqGetString   (seeq_t *);
char      * seeqPrintError  (void);

#define RESET       "\033[0m"
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */

#endif
