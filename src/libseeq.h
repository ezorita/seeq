/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
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

#define LIBSEEQ_VERSION "libseeq-1.1"
#define COLOR_TERMINAL 1

// Match options.
#define SQ_FIRST      0x00
#define SQ_BEST       0x01
#define SQ_ALL        0x02
#define SQ_COUNT      0x03

#define SQ_FAIL       0x00
#define SQ_CONVERT    0x04
#define SQ_IGNORE     0x08

#define SQ_LINES      0x00
#define SQ_STREAM     0x10

#define MASK_MATCH    0x03
#define MASK_NONDNA   0x0C
#define MASK_INPUT    0x10


// Init options
#define INITIAL_MATCH_STACK_SIZE 16

#include <stdio.h>

extern int seeqerr;

typedef struct seeq_t   seeq_t;
typedef struct match_t  match_t;
typedef struct mstack_t  mstack_t;

struct match_t {
   size_t   start;
   size_t   end;
   size_t   dist;
};

struct seeq_t {
   size_t    hits;
   size_t    stacksize;
   match_t * match;
   size_t    bufsz;
   char    * string;
   int       tau;
   int       wlen;
   char    * keys;
   char    * rkeys;
   void    * dfa;
   void    * rdfa;
};

struct mstack_t {
   size_t  size;
   size_t  pos;
   match_t match[];
};


seeq_t     * seeqNew           (const char *, int, size_t);
seeq_t     * seeqNew_keys      (const char *, int, int, size_t);
seeq_t    ** seeqNewSubPattern (const char *, int, int, int *);
void         seeqFree          (seeq_t *);
match_t    * seeqMatchIter     (seeq_t *);
char       * seeqGetString     (seeq_t *);
long         seeqStringMatch   (const char *, seeq_t *, int);
const char * seeqPrintError    (void);
int          seeqAddMatch      (seeq_t *, match_t);
mstack_t   * stackNew          (size_t);
int          stackAddMatch     (mstack_t **, match_t);

// misc functions.
int recursive_merge (size_t, size_t, int, seeq_t *, mstack_t **);


#define RESET       "\033[0m"
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */

#endif
