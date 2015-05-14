#ifndef _SEEQ_H_
#define _SEEQ_H_

#define SEEQ_VERSION "seeq-1.1"

#include <stdlib.h>

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
   int skip;
   size_t memory;
};

int  seeq (char * expression, char * input, struct seeqarg_t args);

#endif
