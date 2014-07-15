#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <execinfo.h>
#include <signal.h>

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
};

struct nstack_t {
   int p;
   int l;
   int val[];
};


int parse(char *, char **);
void proc_match(int *, int, int *, int *);
int update(nfa_t *, char, int);
void setactive(nfa_t *, int, int);
nstack_t * new_stack(int);
void stack_add(nstack_t **, int);

