/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
**
** Last modified: March 2, 2015
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

#ifndef _SEEQCORE_H_
#define _SEEQCORE_H_

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define ABS_MAX_POS        0xFFFFFFFE
#define U32T_ERROR         0xFFFFFFFF
#define INITIAL_STACK_SIZE 256
#define INITIAL_TRIE_SIZE  256
#define INITIAL_DFA_SIZE   256
#define INITIAL_LINE_SIZE  50
#define DFA_FORWARD        1
#define DFA_REVERSE        0
#define DFA_COMPUTE        -1
#define NBASES             5 // Should never be set larger than 32.
#define TRIE_CHILDREN      3

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define type_msb(a) (((size_t)1)<<(sizeof(a)*8-1))
#define set_mintomatch(a) (((uint32_t)(a)) << 16)
#define get_mintomatch(a) (int)((((uint32_t)(a)) >> 16)&0xFFFF)
#define get_match(a) (int)((uint32_t)(a) &0xFFFF)

typedef struct dfa_t    dfa_t;
typedef struct vertex_t vertex_t;
typedef struct edge_t   edge_t;
typedef struct trie_t   trie_t;
typedef struct node_t   node_t;

struct node_t {
   // Type indica si es leaf o un altre node.
   // Si es leaf el que hi ha a child es l'estat
   // de la DFA.
   uint32_t flags;
   uint32_t child[TRIE_CHILDREN];
};

struct trie_t {
   size_t  pos;
   size_t  size;
   size_t  height;
   node_t  nodes[];
};

struct edge_t {
   uint32_t state;
   int match;
};

struct vertex_t {
   // El vertex de la DFA ja no li cal tenir
   // un punter al trie, els vectors d'alineament
   // s'emmagatzemen directament codificats en
   // mallocs individuals.
   uint8_t * align;
   edge_t    next[NBASES];
};

struct dfa_t {
   size_t     pos;
   size_t     size;
   size_t     maxmemory;
   trie_t   * trie;
   int      * align_cache;
   vertex_t   states[];
};

//   [0 ... 255] = 6,
//   ['a'] = 0, ['c'] = 1, ['g'] = 2, ['t'] = 3, ['u'] = 3, ['n'] = 4, ['\0'] = 5,
//   ['A'] = 0, ['C'] = 1, ['G'] = 2, ['T'] = 3, ['U'] = 3, ['N'] = 4, ['\n'] = 5
static const int translate_halt[256] = {
   5,6,6,6,6,6,6,6,6,6, 5,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,
   6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,
   6,6,6,6,6,0,6,1,6,6, 6,2,6,6,6,6,6,6,4,6, 6,6,6,6,3,3,6,6,6,6,
   6,6,6,6,6,6,6,0,6,1, 6,6,6,2,6,6,6,6,6,6, 4,6,6,6,6,6,3,3,6,6,
   6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,
   6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,
   6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,
   6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6,6,6,6,6,
   6,6,6,6,6,6,6,6,6,6, 6,6,6,6,6,6
};

static const int translate_n[256] = {
   5,4,4,4,4,4,4,4,4,4, 5,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4,
   4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4,
   4,4,4,4,4,0,4,1,4,4, 4,2,4,4,4,4,4,4,4,4, 4,4,4,4,3,3,4,4,4,4,
   4,4,4,4,4,4,4,0,4,1, 4,4,4,2,4,4,4,4,4,4, 4,4,4,4,4,4,3,3,4,4,
   4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4,
   4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4,
   4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4,
   4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,4,4,
   4,4,4,4,4,4,4,4,4,4, 4,4,4,4,4,4
};

static const char bases[NBASES] = "ACGTN";

int         parse         (const char *, char *);
dfa_t     * dfa_new       (int, int, size_t, size_t, size_t);
uint32_t    dfa_newvertex (dfa_t **);
int         dfa_newstate  (dfa_t **, uint8_t *, int, size_t); 
int         dfa_step      (uint32_t, int, int, int, dfa_t **, char *, edge_t *);
void        dfa_free      (dfa_t *);
trie_t    * trie_new      (size_t, size_t);
int         trie_search   (dfa_t *, uint8_t *, uint32_t*);
int         trie_insert   (dfa_t *, uint8_t *, uint32_t);
uint32_t    trie_newnode  (trie_t **);
void        trie_reset    (trie_t *);
void        path_to_align (const unsigned char *, int *, size_t);
void        align_to_path (const int *, uint8_t *, size_t);
void        path_encode   (const uint8_t *, uint8_t *, size_t);
void        path_decode   (const uint8_t *, uint8_t *, size_t);
int         path_compare  (const uint8_t *, const uint8_t *, size_t);


#define RESET       "\033[0m"
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */

#endif
