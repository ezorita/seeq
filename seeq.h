#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <execinfo.h>
#include <signal.h>

#define INITIAL_LINE_SIZE 10
#define INITIAL_STACK_SIZE 256
#define INITIAL_TRIE_SIZE  256
#define INITIAL_DFA_SIZE   256

#define NBASES 5

typedef struct nfa_t    nfa_t;
typedef struct nstack_t nstack_t;
typedef struct pstack_t pstack_t;
typedef struct status_t status_t;
typedef struct match_t  match_t;
typedef struct dfa_t    dfa_t;
typedef struct path_t   path_t;
typedef struct btrie_t  btrie_t;
typedef struct bnode_t  bnode_t;

struct nstack_t {
   int p;
   int l;
   int val[];
};

struct status_t {
   int status;
   int match;
};

struct path_t {
   status_t node;
   int      start;
};

struct pstack_t {
   int    p;
   path_t path[];
};

struct match_t {
   int dist;
   int start;
};

struct bnode_t {
   int next[2];
};

struct btrie_t {
   int       pos;
   int       size;
   int       height;
   bnode_t * root;
};

struct dfa_t {
   status_t next[NBASES];
};

static const int translate[256] = {
   [0 ... 255] = 4,
   ['a'] = 0, ['c'] = 1, ['g'] = 2, ['t'] = 3, ['n'] = 4,
   ['A'] = 0, ['C'] = 1, ['G'] = 2, ['T'] = 3, ['N'] = 4
};

static const char bases[NBASES] = "ACGTN";

int parse(char *, char **);
void proc_match(int *, int, int *, int *);
match_t update(char, pstack_t*, pstack_t*, dfa_t*, int, int, int);
void setactive(int, int, int, int, char*, nstack_t**);
nstack_t * new_stack(int);
void stack_add(nstack_t **, int);
pstack_t * new_pstack(int);
void pstack_add(pstack_t *, path_t);
int build_dfa(int, int, int*, int*, dfa_t**, nstack_t*, char*, btrie_t*);
btrie_t * trie_new(int, int);
int trie_search(btrie_t *, char*);
void trie_insert(btrie_t *, char*, int);
void trie_reset(btrie_t *);
void trie_free(btrie_t *);
