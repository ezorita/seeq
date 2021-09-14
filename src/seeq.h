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

#ifndef _SEEQ_H_
#define _SEEQ_H_

#define SEEQ_VERSION "seeq-1.1"

#include "libseeq.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct seeqfile_t seeqfile_t;


struct seeqarg_t {
   int showdist;
   int showpos;
   int showline;
   int printline;
   int matchonly;
   int count;
   int compact;
   int dist;
   int verbose;
   int endline;
   int prefix;
   int split;
   int invert;
   int best;
   int non_dna;
   int all;
   size_t memory;
};

struct seeqfile_t {
   size_t  line;
   FILE  * fdi;
};


// To be moved to seeq.c
#define SQ_ANY        0
#define SQ_MATCH      1
#define SQ_NOMATCH    2
#define SQ_COUNTLINES 3
#define SQ_COUNTMATCH 4

int          seeq            (char *, char *, struct seeqarg_t);
long         seeqFileMatch   (seeqfile_t *, seeq_t *, int, int);
seeqfile_t * seeqOpen        (const char *);
int          seeqClose       (seeqfile_t *);

#endif
