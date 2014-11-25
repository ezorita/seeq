/*
** Copyright 2014 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
**
** Last modified: November 25, 2014
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

#define _GNU_SOURCE

#define VERSION "seeq-1.0"

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
#define INITIAL_LINE_SIZE  50
#define DFA_FORWARD        0
#define DFA_REVERSE        1
#define DFA_COMPUTE        -1
#define NBASES 5 // Should never be set larger than 32.

#define min(a,b) (((a) < (b)) ? (a) : (b))

typedef struct match_t  match_t;
typedef struct dfa_t    dfa_t;
typedef struct vertex_t vertex_t;
typedef struct edge_t   edge_t;
typedef struct trie_t   trie_t;
typedef struct node_t   node_t;

typedef unsigned int uint;

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
};

struct match_t {
   uint dist;
   uint start;
};

struct node_t {
   uint child[3];
   uint parent;
};

struct trie_t {
   uint    pos;
   uint    size;
   uint    height;
   node_t  nodes[];
};

struct edge_t {
   uint state;
   int  match;
};

struct vertex_t {
   uint    node_id;
   edge_t  next[NBASES];
};

struct dfa_t {
   uint     pos;
   uint     size;
   vertex_t states[];
};


static const int translate[256] = {
   [0 ... 255] = 6,
   ['a'] = 0, ['c'] = 1, ['g'] = 2, ['t'] = 3, ['u'] = 3, ['n'] = 4, ['\0'] = 5,
   ['A'] = 0, ['C'] = 1, ['G'] = 2, ['T'] = 3, ['U'] = 3, ['N'] = 4
};

static const char bases[NBASES] = "ACGTN";

void        seeq         (char *, char *, struct seeqarg_t);
int         parse        (char *, char **);
dfa_t     * dfa_new      (uint);
edge_t      dfa_step     (uint, uint, uint, uint, dfa_t **, trie_t **, char *, int);
trie_t    * trie_new     (uint, uint);
uint        trie_search  (trie_t *, char*, uint*, uint*);
uint        trie_insert  (trie_t **, char*, uint, uint);
uint      * trie_getrow  (trie_t *, uint);
void        trie_reset   (trie_t *);

#define RESET   "\033[0m"
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
