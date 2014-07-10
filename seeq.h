#include <stdlib.h>
#include <stdio.h>

typedef struct nfa_t nfa_t;

struct nfa_t {
   int    rows;
   int    cols;
   char * exp;
   int  * act;
};
