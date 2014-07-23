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

#define MEMCHR_PERIOD      200
#define INITIAL_LINE_SIZE  10
#define INITIAL_STACK_SIZE 256
#define INITIAL_TRIE_SIZE  256
#define INITIAL_DFA_SIZE   256
#define DFA_FORWARD        0
#define DFA_REVERSE        1


#define NBASES 5

typedef struct nstack_t nstack_t;
typedef struct status_t status_t;
typedef struct match_t  match_t;
typedef struct dfa_t    dfa_t;
typedef struct btrie_t  btrie_t;
typedef struct bnode_t  bnode_t;

struct seeqarg_t {
   int showdist;
   int showpos;
   int showline;
   int printline;
   int matchonly;
   int count;
   int compact;
   int dist;
};

struct nstack_t {
   int p;
   int l;
   int val[];
};

struct status_t {
   int status;
   int match;
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

void seeq(char *, char *, struct seeqarg_t);
int parse(char *, char **);
void setactive(int, int, int, int, char*, nstack_t**);
nstack_t * new_stack(int);
void stack_add(nstack_t **, int);
int build_dfa(int, int, int*, int*, dfa_t**, nstack_t*, char*, btrie_t*, int);
btrie_t * trie_new(int, int);
int trie_search(btrie_t *, char*);
void trie_insert(btrie_t *, char*, int);
void trie_reset(btrie_t *);
void trie_free(btrie_t *);

#define RESET   "\033[0m"
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
