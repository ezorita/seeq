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
