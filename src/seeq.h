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
#include <pthread.h>

#define INITIAL_STACK_SIZE 256
#define INITIAL_TRIE_SIZE  256
#define INITIAL_DFA_SIZE   256
#define INITIAL_LINE_SIZE  100
#define DFA_FORWARD        0
#define DFA_REVERSE        1
#define DFA_COMPUTE        -1
// Should never be set larger than 32.
#define NBASES 5

// This one defines maxtau
typedef char nfanode_t;

typedef struct state_t   state_t;
typedef struct match_t   match_t;
typedef struct dfa_t     dfa_t;
typedef struct trie_t    trie_t;
typedef struct node_t    node_t;
typedef struct job_t     job_t;
typedef struct jstack_t  jstack_t;
typedef struct sstack_t  sstack_t;
typedef struct mtcont_t  mtcont_t;
typedef struct seeqarg_t seeqarg_t;
typedef struct filepos_t filepos_t;

// Format options
#define OPTION_SHOWDIST  0x00000001
#define OPTION_SHOWLINE  0x00000002
#define OPTION_PRINTLINE 0x00000004
#define OPTION_COUNT     0x00000008
#define OPTION_UNMATCHED 0x00000010
#define OPTION_PRECOMP   0x00000020
#define OPTION_ENDLINE   0x00000040
#define OPTION_REVCOMP   0x00000080
// Let these 3 be the greatest values
#define OPTION_COMPACT   0x00000100
#define OPTION_SHOWPOS   0x00000200
#define OPTION_MATCHONLY 0x00000400
#define OPTION_BEGLINE   0x00000800
#define OPTION_RDFA      OPTION_COMPACT

struct seeqarg_t {
   int             options;
   int             dist;
   unsigned long   isize;
   char          * expr;
   char          * data;
   sstack_t     ** stckin;
   sstack_t     ** stckout;
   mtcont_t      * control;
};

#define MSG_EOF   -1L

struct filepos_t {
   long offset;
   long line;
};

struct sstack_t {
   pthread_mutex_t * mutex;
   pthread_cond_t  * cond;
   int               eof;
   long              p;
   long              l;
   filepos_t         val[];
};

struct mtcont_t {
   pthread_mutex_t * mutex;
   pthread_cond_t  * cond;
   int               nthreads;
};

struct state_t {
   int state;
   int match;
};

struct match_t {
   int dist;
   int start;
};

// Node struct alignment.
// Define this as the number of unsigned ints before next[]
#define NODE_OVERHEAD 1
struct node_t {
   unsigned int parent;
   unsigned int next[];
};

struct trie_t {
   int       pos;
   int       size;
   int       height;
   int       branch;
   node_t  * root;
};

struct dfa_t {
   unsigned int trie_leaf;
   state_t next[NBASES];
};

static const int translate[256] = {
   [0 ... 255] = 6,
   ['a'] = 0, ['c'] = 1, ['g'] = 2, ['t'] = 3, ['n'] = 4, ['\n'] = 5,
   ['A'] = 0, ['C'] = 1, ['G'] = 2, ['T'] = 3, ['N'] = 4
};

static const int translate_reverse[256] = {
   [0 ... 255] = 'X',
   ['a'] = 'T', ['c'] = 'G', ['g'] = 'C', ['t'] = 'A', ['n'] = 'N', ['['] = ']',
   ['A'] = 'T', ['C'] = 'G', ['G'] = 'C', ['T'] = 'A', ['N'] = 'N', [']'] = '['
};

static const char bases[NBASES] = "ACGTN";

void * seeq(void *);
int parse(char *, char **);
dfa_t * build_dfa(int, int, char*, int);
state_t build_dfa_step(int, int, int, int, dfa_t **, trie_t *, char *, int);
trie_t * trie_new(int, int, int);
unsigned int trie_search(trie_t *, char*);
unsigned int * trie_insert(trie_t *, char*, unsigned int);
char * trie_getpath(trie_t *, unsigned int);
void trie_reset(trie_t *);
void trie_free(trie_t *);
sstack_t * new_sstack(int);
void push(sstack_t**, filepos_t);
filepos_t pop(sstack_t**);
void seteof(sstack_t *);
char ** read_expr_file(char*, int*);
char * reverse_pattern(char*);
void epsilon(nfanode_t *, int, int);

// Print definitions
#define STRLEN_LONG      15
#define STRLEN_POSITION  STRLEN_LONG+STRLEN_LONG+2
#define STRLEN_LINENO    STRLEN_LONG+1
#define STRLEN_DISTANCE  3
#define RESET   "\033[0m"
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
