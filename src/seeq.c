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

#include "seeq.h"

void
seeq
(
 char * expression,
 char * input,
 struct seeqarg_t args
)
{
   int verbose = args.verbose;
   //----- PARSE PARAMS -----
   int tau = args.dist;

   if (verbose) fprintf(stderr, "opening input file\n");

   FILE * fdi = (input == NULL ? stdin : fopen(input, "r"));

   if (fdi == NULL) {
      fprintf(stderr,"error: could not open file: %s\n\n", strerror(errno));
      exit(EXIT_FAILURE);
   }
   
   if (verbose) fprintf(stderr, "parsing pattern\n");

   // ----- COMPUTE DFA -----
   char * keys;
   int    wlen = parse(expression, &keys);

   if (!wlen) {
      fprintf(stderr, "error: invalid pattern expression.\n");
      exit(EXIT_FAILURE);
   }
   
   if (tau >= wlen) {
      fprintf(stderr, "error: expression must be longer than the maximum distance.\n");
      exit(EXIT_FAILURE);
   }

   // Reverse the query and build reverse DFA.
   char * rkeys = malloc(wlen);
   for (int i = 0; i < wlen; i++) rkeys[i] = keys[wlen-i-1];

   // Initialize NFA and DFA.
   dfa_t * dfa  = dfa_new(INITIAL_DFA_SIZE);
   dfa_t * rdfa = dfa_new(INITIAL_DFA_SIZE);

   // Initialize state 1.
   edge_t new = {.state = 0, .match = DFA_COMPUTE};
   for (int i = 0; i < NBASES; i++) {
      dfa->states[0].next[i] = new;
      rdfa->states[0].next[i] = new;
   }

   // Initialize tries.
   trie_t * trie  = trie_new(INITIAL_TRIE_SIZE, wlen);
   trie_t * rtrie = trie_new(INITIAL_TRIE_SIZE, wlen);

   // Compute initial NFA state.
   char * path = malloc(wlen+1);
   for (int i = 0; i <= tau; i++) path[i] = 2;
   for (int i = tau + 1; i < wlen; i++) path[i] = 1;

   // Insert initial state into forward DFA.
   uint nodeid = trie_insert(&trie, path, tau+1, 0);
   dfa->states[dfa->pos++].node_id = nodeid;
   // Insert initial state into reverse DFA.
   nodeid = trie_insert(&rtrie, path, tau+1, 0);
   rdfa->states[rdfa->pos++].node_id = nodeid;

   // ----- PROCESS FILE -----
   // Text buffer
   char * data = malloc(INITIAL_LINE_SIZE);
   size_t bufsz = INITIAL_LINE_SIZE;
   ssize_t readsz;
   // Counters.
   unsigned long lines = 0;
   unsigned long count = 0;

   if (verbose) fprintf(stderr, "processing data\n");

   while ((readsz = getline(&data, &bufsz, fdi)) > 0) {
      // Remove newline.
      if (data[readsz-1] == '\n') data[readsz---1] = 0;

      // Reset search variables
      int streak_dist = tau+1;
      int current_node = 0;
      lines++;

      // DFA state.
      for (unsigned long i = 0; i <= readsz; i++) {
         // Update DFA.
         int cin = (int)translate[(int)data[i]];
         edge_t next;
         if(cin < NBASES) {
            next  = dfa->states[current_node].next[cin];
            if (next.match == DFA_COMPUTE)
               next = dfa_step(wlen, tau, current_node, cin, &dfa, &trie, keys, DFA_FORWARD);
            current_node = next.state;
         } else if (cin == 5) {
            next.match = tau+1;
            if (args.invert && streak_dist >= next.match) {
               if (args.showline) fprintf(stdout, "%lu %s\n", lines, data);
               else fprintf(stdout, "%s\n", data);
               break;
            }
         } else {
            break;
         }

         // Update streak.
         if (streak_dist > next.match) {
            // Tau is decreasing, track new streak.
            streak_dist   = next.match;
         } else if (streak_dist < next.match) {
            if (args.invert) break;
            // FORMAT OUTPUT
            if (args.count) {
               count++;
               break;
            } else {
               long j = 0;
               if (args.showpos || args.compact || args.matchonly || args.prefix) {
                  int rnode = 0;
                  int d = tau + 1;
                  // Find match start with RDFA.
                  do {
                     int c = (int)translate[(int)data[i-++j]];
                     edge_t next = rdfa->states[rnode].next[c];
                     if (next.match == DFA_COMPUTE) next = dfa_step(wlen, tau, rnode, c, &rdfa, &rtrie, rkeys, DFA_REVERSE);
                     rnode = next.state;
                     d     = next.match;
                  } while (rnode && d > streak_dist && j < i);

                  // Compute match length and print match.
                  j = i - j;
               }
               if (args.compact) {
                  fprintf(stdout, "%lu:%ld-%ld:%d\n",lines, j, i-1, streak_dist);
               } else {
                  if (args.showline) fprintf(stdout, "%lu ", lines);
                  if (args.showpos)  fprintf(stdout, "%ld-%ld ", j, i-1);
                  if (args.showdist) fprintf(stdout, "%d ", streak_dist);
                  if (args.matchonly) {
                     data[i] = 0;
                     fprintf(stdout, "%s", data+j);
                  } else if (args.printline) {
                     if(isatty(fileno(stdout)) && args.showpos) {
                        char tmp = data[j];
                        data[j] = 0;
                        fprintf(stdout, "%s", data);
                        data[j] = tmp;
                        tmp = data[i];
                        data[i] = 0;
                        fprintf(stdout, (streak_dist > 0 ? BOLDRED : BOLDGREEN));
                        fprintf(stdout, "%s" RESET, data+j);
                        data[i] = tmp;
                        fprintf(stdout, "%s", data+i);
                     } else {
                        fprintf(stdout, "%s", data);
                     }
                  } else {
                     if (args.prefix) {
                        data[j] = 0;
                        fprintf(stdout, "%s", data);
                     }
                     if (args.endline) {
                        fprintf(stdout, "%s", data+i);
                     }
                  }
                  fprintf(stdout, "\n");
               }
            }
            break;
         }
      }
   }
   if (verbose) fprintf(stderr, "done\n");
   if (args.count) fprintf(stdout, "%lu\n", count);
   free(rdfa);
   free(dfa);
   free(data);
}


int
parse
(
 char * expr,
 char ** keysp
)
{
   *keysp = calloc(strlen(expr),sizeof(char));
   char * keys = *keysp;
   int i = 0;
   int l = 0;
   int add = 0;
   char c, lc = 0;
   while((c = expr[i]) != 0) {
      if      (c == 'A' || c == 'a') keys[l] |= 0x01;
      else if (c == 'C' || c == 'c') keys[l] |= 0x02;
      else if (c == 'G' || c == 'g') keys[l] |= 0x04;
      else if (c == 'T' || c == 't') keys[l] |= 0x08;
      else if (c == 'N' || c == 'n') keys[l] |= 0x1F;
      else if (c == '[') {
         if (add) return -1;
         add = 1;
      }
      else if (c == ']') {
         if (!add) return -1;
         if (lc == '[') l--;
         add = 0;
      }
      else return -1;

      if (!add) l++;
      i++;
      lc = c;
   }
   
   if (add == 1) return -1;
   else return l;
}

dfa_t *
dfa_new
(
 int vertices
)
{
   if (vertices < 1) vertices = 1;
   dfa_t * dfa = malloc(sizeof(dfa_t) + vertices * sizeof(vertex_t));
   if (dfa == NULL) {
      fprintf(stderr, "error in dfa_new (malloc): %s.\n", strerror(errno));
      exit(EXIT_FAILURE);
   }
   dfa->size = vertices;
   dfa->pos  = 0;

   return dfa;
}


edge_t
dfa_step
(
 uint      plen,
 uint      tau,
 uint      dfa_state,
 uint      base,
 dfa_t  ** dfap,
 trie_t ** trie,
 char   *  exp,
 int       anchor
)
{
   dfa_t  * dfa   = *dfap;
   int      value = 1 << base;

   // Explore the trie backwards to find out the NFA state.
   uint * state = trie_getrow(*trie, dfa->states[dfa_state].node_id);
   char * code  = calloc(plen + 1, sizeof(char));

   // Initialize first column.
   int nextold, prev, old = state[0];
   if (anchor) {
      state[0] = prev = min(tau + 1, state[0] + 1);
   } else {
      state[0] = prev = 0;
   }

   // Update row.
   for (int i = 1; i < plen+1; i++) {
      nextold   = state[i];
      state[i]  = min(tau + 1, min(old + ((value & exp[i-1]) == 0), min(prev, state[i]) + 1));
      code[i-1] = state[i] - prev + 1;
      prev      = state[i];
      old       = nextold;
   }

   // Save match value.
   dfa->states[dfa_state].next[base].match = prev;
   
   // Check if this state already exists.
   uint dfalink;
   int exists = trie_search(*trie, code, NULL, &dfalink);

   if (exists) {
      // If exists, just link with the existing state.
      dfa->states[dfa_state].next[base].state = dfalink;
   } else {
      // Insert new NFA state in the trie.
      uint nodeid = trie_insert(trie, code, prev, dfa->pos);
      // Create new vertex in dfa network.
      uint vertexid = dfa_newvertex(dfap, nodeid);
      dfa = *dfap;
      // Connect dfa vertices.
      dfa->states[dfa_state].next[base].state = vertexid;
   }

   free(state);
   free(code);
   
   return dfa->states[dfa_state].next[base];
}

uint
dfa_newvertex
(
 dfa_t ** dfap,
 uint     nodeid
)
{
   dfa_t * dfa = *dfap;

   // Create new vertex in DFA network.
   if (dfa->pos >= dfa->size) {
      dfa->size *= 2;
      *dfap = dfa = realloc(dfa, sizeof(dfa_t) + dfa->size * sizeof(vertex_t));
      if (dfa == NULL) {
         fprintf(stderr, "error (realloc) dfa in 'dfa_newnode': %s\n", strerror(errno));
         exit(EXIT_FAILURE);
      }
   }

   // Initialize DFA vertex.
   dfa->states[dfa->pos].node_id = nodeid;
   edge_t new = {.state = 0, .match = DFA_COMPUTE};
   for (int j = 0; j < NBASES; j++) dfa->states[dfa->pos].next[j] = new;

   // Increase counter.
   return dfa->pos++;
}

trie_t *
trie_new
(
 int initial_size,
 int height
)
{

   // Allocate at least one node.
   if (initial_size < 1) initial_size = 1;
   if (height < 0) height = 0;

   trie_t * trie = malloc(sizeof(trie_t) + initial_size*sizeof(node_t));
   if (trie == NULL) {
      fprintf(stderr, "error (malloc) trie_t in trie_new: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
   }

   // Initialize root node.
   memset(&(trie->nodes[0]), 0, initial_size*sizeof(node_t));

   // Initialize trie struct.
   trie->pos = 1;
   trie->size = initial_size;
   trie->height = height;

   return trie;
}

uint
trie_search
(
 trie_t * trie,
 char   * path,
 uint   * value,
 uint   * dfastate
 )
{
   uint id = 0;
   for (int i = 0; i < trie->height; i++) {
      id = trie->nodes[id].child[(int)path[i]];
      if (id == 0) return 0;
   }
   // Save leaf value.
   if (value != NULL) *value = trie->nodes[id].child[0];
   if (dfastate != NULL) *dfastate = trie->nodes[id].child[1];
   return 1;
}


uint *
trie_getrow
(
 trie_t * trie,
 uint     nodeid
)
{
   node_t * nodes = &(trie->nodes[0]);
   uint   * path  = malloc((trie->height + 1) * sizeof(uint));
   int      i     = trie->height;
   uint     id    = nodeid;

   // Match value.
   path[i] = nodes[id].child[0];
   
   while (id != 0 && i > 0) {
      uint next_id = nodes[id].parent;
      path[i-1] = path[i] + (nodes[next_id].child[0] == id) - (nodes[next_id].child[2] == id);
      id = next_id;
      i--;
   } 

   // Control.
   if (i != 0) {
      free(path);
      return NULL;
   }

   return path;
}


uint
trie_insert
(
 trie_t ** triep,
 char   *  path,
 uint      value,
 uint      dfastate
)
{
   trie_t * trie  = *triep;
   node_t * nodes = &(trie->nodes[0]);
   uint id = 0;

   int i;
   for (i = 0; i < trie->height; i++) {
      if (path[i] < 0 || path[i] >= TRIE_CHILDREN)
         return 0;
      // Walk the tree.
      if (nodes[id].child[(int)path[i]] != 0) {
         id = nodes[id].child[(int)path[i]];
         continue;
      }

      // Create new node.
      if (trie->pos >= trie->size) {
         size_t newsize = trie->size * 2;
         *triep = trie = realloc(trie, sizeof(trie_t) + newsize * sizeof(node_t));
         if (trie == NULL) {
            fprintf(stderr, "error (realloc) in trie_insert: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
         }
         // Update pointers.
         nodes = &(trie->nodes[0]);
         // Initialize new nodes.
         trie->size = newsize;
      }
      
      // Consume one node of the trie.
      uint newid = trie->pos;
      nodes[newid].parent = id;
      nodes[newid].child[0] = nodes[newid].child[1] = nodes[newid].child[2] = 0;
      nodes[id].child[(int)path[i]] = newid;
      trie->pos++;

      // Go one level deeper.
      id = newid;
   }

   // Write data.
   nodes[id].child[0] = value;
   nodes[id].child[1] = dfastate;

   return id;
}


void
trie_reset
(
 trie_t * trie
 )
{
   trie->pos = 0;
   memset(&(trie->nodes[0]), 0, sizeof(node_t));
}
