#include <stdlib.h>
#include <stdio.h>

#define INITIAL_LINE_SIZE 10
#define INITIAL_STACK_SIZE 256

typedef struct nfa_t nfa_t;
typedef struct nstack_t nstack_t;

struct nfa_t {
   int        rows;
   int        cols;
   char     * exp;
   int      * act;
   nstack_t * stack;
   nstack_t * temp;
   mbuf_t   * mbuf;
};

struct nstack_t {
   int p;
   int l;
   int val[];
};

struct mbuf_t {
   int p;
   int m[];
};
