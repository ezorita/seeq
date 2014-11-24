#include "seeq.h"

// TODO:
// - Update seeq function to use the new tree and nfa_build functions.
// - Update seeq.h to match the new headers.
// - Solve other TODOS in the file.

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
   if (input == NULL) {
      fprintf(stderr, "error: input file not specified.\n");
      exit(1);
   }

   if (verbose) fprintf(stderr, "loading input file\n");

   int fdi = open(input, O_RDWR);

   if (fdi == -1) {
      fprintf(stderr,"error: could not open file: %s\n\n", strerror(errno));
      exit(1);
   }

   unsigned long isize = lseek(fdi, 0, SEEK_END);
   lseek(fdi, 0, SEEK_SET);
   
   // Map file to memory.
   char * data = (char *) mmap(NULL, isize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fdi, 0);

   if (data == MAP_FAILED) {
      fprintf(stderr, "error loading data into memory (mmap): %s\n", strerror(errno));
      exit(EXIT_FAILURE);
   }

   if (verbose) fprintf(stderr, "parsing pattern\n");

   // ----- COMPUTE DFA -----
   char * keys;
   int    wlen = parse(expression, &keys);

   if (!wlen) {
      fprintf(stderr, "error: invalid pattern expression.\n");
      exit(1);
   }
   
   if (tau >= wlen) {
      fprintf(stderr, "error: expression must be longer than the maximum distance.\n");
      exit(1);
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

   //DEBUG
   /*
      fprintf(stdout, "\t\t");
   for (int i = 0; i < wlen;  i++) fprintf(stdout, "\t%c", expression[i]);
   fprintf(stdout, "\n0\t");
   for (int i = 0; i <= tau; i++) fprintf(stdout, "\t%d",i);
   for (int i = tau+1; i < wlen+1; i++) fprintf(stdout, "\t%d", tau+1);
   fprintf(stdout, "\n");
   */

   // ----- PROCESS FILE -----
   // Read and update
   int streak_dist = tau+1;
   int current_node = 0;
   long i = 0;
   unsigned long lines = 1;
   unsigned long linestart = 0;
   unsigned long count = 0;

   if (verbose) fprintf(stderr, "processing data\n");

   // DFA state.
   for (unsigned long k = 0; k < isize; k++, i++) {
      // Update DFA.
      int cin = (int)translate[(int)data[k]];
      edge_t next;
      if(cin < 5) {
         next  = dfa->states[current_node].next[cin];
         if (next.match == DFA_COMPUTE) next = dfa_step(wlen, tau, current_node, cin, &dfa, &trie, keys, DFA_FORWARD);
         //DEBUG
         //                 else fprintf(stdout, "%d->%d\t%c\n", current_node, next.state, data[k]);
         current_node = next.state;
      } else {
         next.match = tau+1;
      }

         // Update streak.
         if (streak_dist > next.match) {
            // Tau is decreasing, track new streak.
            streak_dist   = next.match;
         } else if (streak_dist < next.match) { 
            while (data[k] != '\n' && k < isize) k++;
            data[k] = 0;

            // FORMAT OUTPUT (quite crappy)
            if (args.count) {
               count++;
            } else {
               long j = 0;
               if (args.showpos || args.compact || args.matchonly) {
                  int rnode = 0;
                  int d = tau + 1;
                  // Find match start with RDFA.
                  do {
                     int c = (int)translate[(int)data[linestart+i-++j]];
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
                     data[linestart+i] = 0;
                     fprintf(stdout, "%s", data+linestart+j);
                  } else if (args.endline) {
                     fprintf(stdout, "%s", data+linestart+i);
                  } else if (args.printline) {
                     if(isatty(fileno(stdout)) && args.showpos) {
                        char tmp = data[linestart + j];
                        data[linestart+j] = 0;
                        fprintf(stdout, "%s", data+linestart);
                        data[linestart+j] = tmp;
                        tmp = data[linestart+i];
                        data[linestart+i] = 0;
                        fprintf(stdout, (streak_dist > 0 ? BOLDRED : BOLDGREEN));
                        fprintf(stdout, "%s" RESET, data+linestart+j);
                        data[linestart+i] = tmp;
                        fprintf(stdout, "%s", data+linestart+i);
                     } else {
                        fprintf(stdout, "%s", data+linestart);
                     }
                  }
                  fprintf(stdout, "\n");
               }
            }
            data[k] = '\n';
         }


      if (data[k] == '\n') {
         linestart = k + 1;
         i = -1;
         lines++;
         current_node = 0;
         streak_dist = tau + 1;
      }

   }

   if (args.count) fprintf(stdout, "%lu\n", count);
   free(rdfa);
   free(dfa);
}


int
parse
(
 char * expr,
 char ** keysp
)
{
   // FIXME: expressions containing unmatched closing brackets
   // do not return -1 as they ought to. Either check the matching
   // of brackets or use the IUPAC alphabet for degenerate positions.
   *keysp = calloc(strlen(expr),sizeof(char));
   char * keys = *keysp;
   int i = 0;
   int l = 0;
   int add = 0;
   char c;
   while((c = expr[i]) != 0) {
      if      (c == 'A' || c == 'a') keys[l] |= 0x01;
      else if (c == 'C' || c == 'c') keys[l] |= 0x02;
      else if (c == 'G' || c == 'g') keys[l] |= 0x04;
      else if (c == 'T' || c == 't') keys[l] |= 0x08;
      else if (c == 'N' || c == 'n') keys[l] |= 0x1F;
      else if (c == '[') add = 1;
      else if (c == ']') add = 0;
      else return 0;

      if (!add) l++;
      i++;
   }
   
   if (add == 1) return 0;
   else return l;
}

dfa_t *
dfa_new
(
 uint vertices
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
      state[0] = prev = state[0] + 1;
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

   //DEBUG
   /*
   char values[] = {'A','C','G','T','N'};
   fprintf(stdout, "%d->%d\t%c", dfa_state, (exists ? dfalink : dfa->pos), values[base]);
   for (int i = 0; i < plen+1; i++) fprintf(stdout, "\t%d", state[i]);
   if (prev <= tau) fprintf(stdout, " *");
   fprintf(stdout, "\n");
   */

   if (exists) {
      // If exists, just link with the existing state.
      dfa->states[dfa_state].next[base].state = dfalink;
   } else {
      // Insert new NFA state in the trie.
      uint nodeid = trie_insert(trie, code, prev, dfa->pos);

      // Create new vertex in DFA network.
      if (dfa->pos >= dfa->size) {
         dfa->size *= 2;
         *dfap = dfa = realloc(dfa, sizeof(dfa_t) + dfa->size * sizeof(vertex_t));
         if (dfa == NULL) {
            fprintf(stderr, "error (realloc) dfa in 'build_dfa_step': %s\n", strerror(errno));
            exit(1);
         }
      }

      // Initialize DFA vertex.
      dfa->states[dfa->pos].node_id = nodeid;
      edge_t new = {.state = 0, .match = DFA_COMPUTE};
      for (int j = 0; j < NBASES; j++) dfa->states[dfa->pos].next[j] = new;

      // Connect vertices.
      dfa->states[dfa_state].next[base].state = dfa->pos;
      
      // Increase counter.
      dfa->pos++;
   }

   free(state);
   free(code);
   
   return dfa->states[dfa_state].next[base];
}

trie_t *
trie_new
(
 uint initial_size,
 uint height
)
{

   // Allocate at least one node.
   if (initial_size < 1) initial_size = 1;

   trie_t * trie = malloc(sizeof(trie_t) + initial_size*sizeof(node_t));
   if (trie == NULL) {
      fprintf(stderr, "error (malloc) trie_t in trie_new: %s\n", strerror(errno));
      exit(1);
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
      if (path[i] > 2) return 0;
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
            exit(1);
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
