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

int
seeq
(
 char * expression,
 char * input,
 struct seeqarg_t args
)
// SYNOPSIS:                                                              
//   Sequentially searches for a given pattern in the file name
//   passed in "input", or from the standard input if the input argu-
//   ment is set to NULL. The output generated is sent to the standard
//   output and depends on the flags enabled in the 'args' struct.
//   The matching metric is the Levenshtein distance. The input sequences
//   must be a succession of DNA/RNA nucleotides ('A','C','T','G','U','N')
//   separated by newline characters '\n'.
//                                                                        
// PARAMETERS:                                                            
//   expression: the pattern to match.
//   input: string containing the filename or NULL pointer for stdin.
//   args: properly filled seeqarg_t struct to customize the output format.
//     struct seeqarg_t:
//     - showdist: Prints the levenshtein distance of the match.
//     - showpos: Prints the position (stard-end) of the matched nucleotides.
//     - showline: Prints the line number of the matched line.
//     - printline: Prints the matched line.
//     - matchonly: Prints only the matched nucleotides.
//     - count: The only output is the count of matched lines.
//     - compact: Prints matches in compact format.
//     - dist: Match distance threshold.
//     - verbose: Print verbose messages.
//     - endline: Prints only the end of the line starting after the match.
//     - prefix: Prints only the beginnig of the line ending before the match.
//     - invert: Prints only the non-matched lines.
//     ** All format options are enabled setting its value to 1, except dist,
//     ** which must contain a positive integer value.
//                                                                        
// RETURN:                                                                
//   seeq returns 0 on success, or -1 if an error occurred.
//
// SIDE EFFECTS:
//   None.
{
   int verbose = args.verbose;
   //----- PARSE PARAMS -----
   int tau = args.dist;

   if (verbose) fprintf(stderr, "opening input file\n");

   FILE * fdi = (input == NULL ? stdin : fopen(input, "r"));

   if (fdi == NULL) {
      fprintf(stderr,"error: could not open file: %s\n\n", strerror(errno));
      return EXIT_FAILURE;
   }
   
   if (verbose) fprintf(stderr, "parsing pattern\n");

   // ----- INITIALIZE DFA AND NFA -----
   char keys[strlen(expression)];
   int  wlen = parse(expression, keys);

   if (wlen == -1) {
      fprintf(stderr, "error: invalid pattern expression.\n");
      return EXIT_FAILURE;
   }
   
   if (tau >= wlen) {
      fprintf(stderr, "error: expression must be longer than the maximum distance.\n");
      return EXIT_FAILURE;
   }

   if (tau < 0) {
      fprintf(stderr, "error: invalid distance.\n");
      return EXIT_FAILURE;
   }

   // Reverse the query and build reverse DFA.
   char rkeys[wlen];
   for (int i = 0; i < wlen; i++) rkeys[i] = keys[wlen-i-1];

   // Initialize DFA and RDFA graphs.
   dfa_t * dfa  = dfa_new(INITIAL_DFA_SIZE);
   if (dfa == NULL) return EXIT_FAILURE;
   dfa_t * rdfa = dfa_new(INITIAL_DFA_SIZE);
   if (rdfa == NULL) return EXIT_FAILURE;

   // Initialize tries.
   trie_t * trie  = trie_new(INITIAL_TRIE_SIZE, wlen);
   if (trie == NULL) return EXIT_FAILURE;
   trie_t * rtrie = trie_new(INITIAL_TRIE_SIZE, wlen);
   if (rtrie == NULL) return EXIT_FAILURE;

   // Compute initial NFA state.
   char path[wlen+1];
   for (int i = 0; i <= tau; i++) path[i] = 2;
   for (int i = tau + 1; i < wlen; i++) path[i] = 1;

   // Insert initial state into forward DFA.
   uint nodeid = trie_insert(&trie, path, tau+1, 0);
   if (nodeid == (uint)-1) return EXIT_FAILURE;
   dfa->states[0].node_id = nodeid;
   // Insert initial state into reverse DFA.
   nodeid = trie_insert(&rtrie, path, tau+1, 0);
   if (nodeid == (uint)-1) return EXIT_FAILURE;
   rdfa->states[0].node_id = nodeid;

   // ----- PROCESS FILE -----
   // Text buffer
   char * data = malloc(INITIAL_LINE_SIZE);
   if (data == NULL) {
      fprintf(stderr, "error in 'seeq' (malloc) data: %s\n", strerror(errno));
      return EXIT_FAILURE;
   }
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
            next = dfa->states[current_node].next[cin];
            if (next.match == DFA_COMPUTE)
               if (dfa_step(current_node, cin, wlen, tau, &dfa, &trie, keys, DFA_FORWARD, &next))
                  return EXIT_FAILURE;
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
          
   // ----- FORMAT OUTPUT -----
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
                     if (next.match == DFA_COMPUTE)
                        if (dfa_step(rnode, c, wlen, tau, &rdfa, &rtrie, rkeys, DFA_REVERSE, &next))
                           return EXIT_FAILURE;
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

   // ----- FREE MEMORY -----
   free(rdfa);
   free(dfa);
   free(data);

   return EXIT_SUCCESS;
}


int
parse
(
 char * expr,
 char * keys
)
// SYNOPSIS:                                                              
//   Parses a pattern expression converting them into bytes, one for each
//   matching position. The key byte is defined as follows:
//
//      bit0 (LSB): match 'A' or 'a'.
//      bit1      : match 'C' or 'c'.
//      bit2      : match 'G' or 'g'.
//      bit3      : match 'T', 't', 'U' or 'u'.
//      bit4      : match 'N' or 'n'.
//      bit5..7   : not used.
//
//   Each key may have one or more bits set if different bases are allowed
//   to match a single position. The expected input expression is a string
//   containing nucleotides 'A','C','G','T','U' and 'N'. If one matching
//   position allows more than one base, the matching bases must be enclosed
//   in square brackets '[' ']'. For instance,
//
//      AC[AT]
//
//   would match a sequence containing A in the 1st position, C in the 2nd
//   position and either A or T in the 3rd position. The associated keys
//   would be
//
//      keys("AC[AT]") = {0x01, 0x02, 0x09}
//
//   If the expression contains 'N', that position will match any nucleotide,
//   including 'N'.
//                                                                        
// PARAMETERS:                                                            
//   expr: A null-terminated string containing the matching expression.
//   keys: pointer to a previously allocated vector of chars. The size of the
//         allocated region should be equal to strlen(expr).
//                                                                        
// RETURN:                                                                
//   On success, parse returns the number of matching positions (keys) for the
//   given expression, or -1 if an error occurred.
//
// SIDE EFFECTS:
//   The contents of the pointer 'keys' are overwritten.
{
   // Initialize keys to 0.
   for (int i = 0; i < strlen(expr); i++) keys[i] = 0;

   int i = 0;
   int l = 0;
   int add = 0;
   char c, lc = 0;
   while((c = expr[i]) != 0) {
      if      (c == 'A' || c == 'a') keys[l] |= 0x01;
      else if (c == 'C' || c == 'c') keys[l] |= 0x02;
      else if (c == 'G' || c == 'g') keys[l] |= 0x04;
      else if (c == 'T' || c == 't' || c == 'U' || c == 'u') keys[l] |= 0x08;
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
// SYNOPSIS:                                                              
//   Creates and initializes a new dfa graph with a root vertex and the specified
//   number of preallocated vertices. 
//                                                                        
// PARAMETERS:                                                            
//   vertices: the number of preallocated vertices.
//                                                                        
// RETURN:                                                                
//   On success, the function returns a pointer to the new dfa_t structure.
//   A NULL pointer is returned in case of error.
//
// SIDE EFFECTS:
//   The returned dfa_t struct is allocated using malloc and must be manually freed.
{
   if (vertices < 1) vertices = 1;
   dfa_t * dfa = malloc(sizeof(dfa_t) + vertices * sizeof(vertex_t));
   if (dfa == NULL) {
      fprintf(stderr, "error in 'dfa_new' (malloc) dfa_t: %s.\n", strerror(errno));
      return NULL;
   }

   // Initialize state 0.
   edge_t new = {.state = 0, .match = DFA_COMPUTE};
   for (int i = 0; i < NBASES; i++) {
      dfa->states[0].next[i] = new;
   }

   dfa->size = vertices;
   dfa->pos  = 1;

   return dfa;
}


int
dfa_step
(
 uint      dfa_state,
 uint      base,
 uint      plen,
 uint      tau,
 dfa_t  ** dfap,
 trie_t ** trie,
 char   *  exp,
 int       anchor,
 edge_t *  nextedge
)
// SYNOPSIS:                                                              
//   Updates the current status of DFA graph. Based on the parameters passed,
//   the function will compute the next row of the Needleman-Wunsch matrix and
//   update the graph accordingly. If the new row points to an already-known
//   state, the two existing vertices will be linked with an edge. Otherwise,
//   a new vertex will be created for the new row.
//                                                                        
// PARAMETERS:                                                            
//   dfa_state : current state of the DFA.
//   base      : next base to resolve (0 for 'A', 1 for 'C', 2 for 'G', '3' for 'T'/'U' and 4 for 'N).
//   plen      : length of the pattern, as returned by parse.
//   tau       : Levenshtein distance threshold.
//   dfap      : pointer to a memory space containing the address of the DFA.
//   trie      : pointer to a memory space containing the address of the associated trie.
//   exp       : expression keys, as returned by parse.
//   anchor    : if set to 1, the first NW column is always initialized with 0.
//   nextedge  : a pointer to an edge_t struct where the computed transition will be placed,
//                                                                        
// RETURN:                                                                
//   dfa_step returns 0 on success, or -1 if an error occurred.
//
// SIDE EFFECTS:
//   The returned dfa_t struct is allocated using malloc and must be manually freed.
{
   dfa_t  * dfa   = *dfap;

   // Return next vertex if already computed.
   if (dfa->states[dfa_state].next[base].match != DFA_COMPUTE) {
      *nextedge = dfa->states[dfa_state].next[base];
      return 0;
   }

   int  value = 1 << base;

   // Explore the trie backwards to find out the NFA state.
   uint * state = trie_getrow(*trie, dfa->states[dfa_state].node_id);
   char * code  = calloc(plen + 1, sizeof(char));

   // Initialize first column.
   int nextold, prev, old = state[0];
   if (anchor) {
      state[0] = prev = 0;
   } else {
      state[0] = prev = min(tau + 1, state[0] + 1);
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

   if (exists == 1) {
      // If exists, just link with the existing state.
      dfa->states[dfa_state].next[base].state = dfalink;
   } else if (exists == 0) {
      // Insert new NFA state in the trie.
      uint nodeid = trie_insert(trie, code, prev, dfa->pos);
      if (nodeid == (uint)-1) return -1;
      // Create new vertex in dfa graph.
      uint vertexid = dfa_newvertex(dfap, nodeid);
      if (vertexid == (uint)-1) return -1;
      dfa = *dfap;
      // Connect dfa vertices.
      dfa->states[dfa_state].next[base].state = vertexid;
   } else {
      fprintf(stderr, "error in 'trie_search': incorrect path.\n");
      return -1;
   }

   free(state);
   free(code);
   
   *nextedge = dfa->states[dfa_state].next[base];
   return 0;
}


uint
dfa_newvertex
(
 dfa_t ** dfap,
 uint     nodeid
)
// SYNOPSIS:                                                              
//   Adds a new vertex to the dfa graph. The new vertex is not linked to any other
//   vertex in any way, so the connection must be done manually.
//                                                                        
// PARAMETERS:                                                            
//   dfap   : pointer to a memory space containing the address of the DFA.
//   nodeid : id of the associated node in the NW row trie, i.e. where the
//            NW row corresponding to this state is stored.
//                                                                        
// RETURN:                                                                
//   On success, the function returns the id of the new vertex. In case of error
//   the function returns (uint)-1.
//
// SIDE EFFECTS:
//   If the dfa has reached its maximum size, the dfa will be reallocated
//   doubling its current size. The address of the dfa may have changed after
//   calling dfa_newvertex.
{
   dfa_t * dfa = *dfap;

   // Create new vertex in DFA graph.
   if (dfa->pos >= dfa->size) {
      dfa->size *= 2;
      *dfap = dfa = realloc(dfa, sizeof(dfa_t) + dfa->size * sizeof(vertex_t));
      if (dfa == NULL) {
         fprintf(stderr, "error in 'dfa_newnode' (realloc) dfa_t: %s\n", strerror(errno));
         return -1;
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
// SYNOPSIS:                                                              
//   Creates and initializes a new NW-row trie preallocated with the specified
//   number of nodes.
//                                                                        
// PARAMETERS:                                                            
//   initial_size : the number of preallocated nodes.
//   height       : height of the trie. It must be equal to the number of keys
//                  as returned by parse.
//                                                                        
// RETURN:                                                                
//   On success, the function returns a pointer to the new trie_t structure.
//   A NULL pointer is returned in case of error.
//
// SIDE EFFECTS:
//   The returned trie_t struct is allocated using malloc and must be manually freed.
{

   // Allocate at least one node.
   if (initial_size < 1) initial_size = 1;
   if (height < 0) height = 0;

   trie_t * trie = malloc(sizeof(trie_t) + initial_size*sizeof(node_t));
   if (trie == NULL) {
      fprintf(stderr, "error in 'trie_new' (malloc) trie_t: %s\n", strerror(errno));
      return NULL;
   }

   // Initialize root node.
   memset(&(trie->nodes[0]), 0, initial_size*sizeof(node_t));

   // Initialize trie struct.
   trie->pos = 1;
   trie->size = initial_size;
   trie->height = height;

   return trie;
}


int
trie_search
(
 trie_t * trie,
 char   * path,
 uint   * value,
 uint   * dfastate
 )
// SYNOPSIS:                                                              
//   Searches the trie following a specified path and returns the values stored
//   at the leaf.
//                                                                        
// PARAMETERS:                                                            
//   trie     : Pointer to the trie.
//   path     : The path as an array of chars containing values {0,1,2}
//   value    : Pointer where the leaf value will be placed (if found).
//   dfastate : Pointer where the leaf dfastate will be placed (if found).
//                                                                        
// RETURN:                                                                
//   trie_search returns 1 if the path was found, and 0 otherwise. If an
//   error occurred during the search, -1 is returned.
//
// SIDE EFFECTS:
//   None.
{
   uint id = 0;
   for (int i = 0; i < trie->height; i++) {
      if (path[i] >= TRIE_CHILDREN || path[i] < 0) return -1;
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
// SYNOPSIS:                                                              
//   Recomputes the NW row that terminates at nodeid ('nodeid' must point
//   to a trie leaf).
//                                                                        
// PARAMETERS:                                                            
//   trie   : Pointer to the trie.
//   nodeid : Id of the leaf at which the NW row terminates.
//                                                                        
// RETURN:                                                                
//   trie_getrow returns a pointer to the start of the NW row. If an
//   error occurred during the row computation or nodeid did not point
//   to a leaf node, a NULL pointer is returned.
//
// SIDE EFFECTS:
//   An array containing the NW row is allocated using malloc and must be
//   manually freed.
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
// SYNOPSIS:                                                              
//   Inserts the specified path in the trie and stores the end value and
//   the dfa state at the leaf (last node of the path). If the path already
//   exists, its leaf values will be overwritten.
//                                                                        
// PARAMETERS:                                                            
//   trie      : pointer to a memory space containing the address of the trie.
//   path     : The path as an array of chars containing values {0,1,2}
//   value    : The value of the last column of the NW row.
//   dfastate : The NW-row-associated DFA state. 
//                                                                        
// RETURN:                                                                
//   On success, dfa_insert returns the id of the leaf where the values were
//   stored, -1 is returned if an error occurred.
//
// SIDE EFFECTS:
//   If the trie has reached its limit of allocated nodes, it will be reallocated
//   doubling its size. The address of the trie may have changed after calling dfa_insert.
{
   trie_t * trie  = *triep;
   node_t * nodes = &(trie->nodes[0]);
   uint id = 0;
   uint initial_pos = trie->pos;

   int i;
   for (i = 0; i < trie->height; i++) {
      if (path[i] < 0 || path[i] >= TRIE_CHILDREN) {
         // Bad path, revert trie and return.
         trie->pos = initial_pos;
         return -1;
      }
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
            fprintf(stderr, "error in 'trie_insert' (realloc) trie_t: %s\n", strerror(errno));
            return -1;
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
// SYNOPSIS:                                                              
//   Resets the trie by pruning the root node. The size of the trie, in terms
//   of preallocated nodes is maintained.
//                                                                        
// PARAMETERS:                                                            
//   trie   : Pointer to the trie.
//                                                                        
// RETURN:                                                                
//   void.
//
// SIDE EFFECTS:
//   None.
{
   trie->pos = 0;
   memset(&(trie->nodes[0]), 0, sizeof(node_t));
}
