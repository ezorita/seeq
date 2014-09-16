#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#define INITIAL_STACK_SIZE 256
#define INITIAL_TRIE_SIZE  256
#define INITIAL_DFA_SIZE   256
#define DFA_FORWARD        0
#define DFA_REVERSE        1
#define DFA_COMPUTE        -1
// Should never be set larger than 32.
#define NBASES 5

typedef struct state_t  state_t;
typedef struct match_t  match_t;
typedef struct dfa_t    dfa_t;
typedef struct btrie_t  btrie_t;
typedef struct bnode_t  bnode_t;
typedef struct job_t    job_t;
typedef struct jstack_t  jstack_t;

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
   int precompute;
   int endline;
};

struct state_t {
   int state;
   int match;
};

struct match_t {
   int dist;
   int start;
};

struct node_t {
   int      value;
   node_t * parent;
   node_t * next[];
};

struct trie_t {
   int       pos;
   int       size;
   int       height;
   int       branch;
   node_t  * root;
};

struct dfa_t {
   node_t * trie_leaf;
   state_t  next[NBASES];
};

static const int translate[256] = {
   [0 ... 255] = 6,
   ['a'] = 0, ['c'] = 1, ['g'] = 2, ['t'] = 3, ['n'] = 4, ['\n'] = 5,
   ['A'] = 0, ['C'] = 1, ['G'] = 2, ['T'] = 3, ['N'] = 4
};

static const char bases[NBASES] = "ACGTN";

void seeq(char *, char *, struct seeqarg_t);
int parse(char *, char **);
int setactive(int, int, int, char*);
jstack_t * new_jstack(int);
void push(jstack_t **, job_t);
job_t pop(jstack_t *);
dfa_t * build_dfa(int, int, char*, int);
state_t build_dfa_step(int, int, int, int, dfa_t **, btrie_t *, char *, int);
trie_t * trie_new(int, int, int);
unsigned int trie_search(trie_t *, int*);
unsigned int * trie_insert(trie_t *, int*, unsigned int);
int * trie_getpath(trie_t *, unsigned int);
void trie_reset(trie_t *);
void trie_free(trie_t *);

#define RESET   "\033[0m"
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
