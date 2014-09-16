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
   int    rows = tau + 1;
   int    cols = wlen + 1;
   int    nstat= cols*rows;

   if (!wlen) {
      fprintf(stderr, "error: invalid pattern expression.\n");
      exit(1);
   }
   
   if (rows >= cols) {
      fprintf(stderr, "error: expression must be longer than the maximum distance.\n");
      exit(1);
   }

   if (verbose) fprintf(stderr, "building DFA\n");

   // Reverse the query and build reverse DFA.
   char * rkeys = malloc(wlen);
   for (int i = 0; i < wlen; i++)
      rkeys[i] = keys[wlen-i-1];

   dfa_t * dfa, * rdfa;
   btrie_t * trie, * rtrie;
   if (!args.precompute) {
      dfa = malloc(INITIAL_DFA_SIZE * sizeof(dfa_t));
      rdfa = malloc(INITIAL_DFA_SIZE * sizeof(dfa_t));
      // Initialize R/DFA for step building.
      int * dfa_st = (int *) dfa;
      int * dfa_sz = dfa_st + 1;
      int * rdfa_st = (int *) rdfa;
      int * rdfa_sz = rdfa_st + 1;
      *dfa_st = *rdfa_st =  0;
      *dfa_sz = *rdfa_sz = INITIAL_DFA_SIZE;
      // Initialize state 1.
      state_t new = {.state = DFA_COMPUTE, .match = rows};
      for (int i = 0; i < NBASES; i++) {
         dfa[1].next[i] = new;
         rdfa[1].next[i] = new;
      }
      // Initialize tries.
      trie  = trie_new(INITIAL_TRIE_SIZE, nstat - rows);
      rtrie = trie_new(INITIAL_TRIE_SIZE, nstat - rows);
      // Insert state 1.
      char * matrix = calloc(nstat-rows, sizeof(char));
      setactive(rows, cols, 0, matrix);
      unsigned int * leafp = trie_insert(trie, matrix, ++(*dfa_st));
      unsigned int leaf = leafp - (unsigned int *) trie->root; // bnode_t has 3 ints.
      dfa[1].trie_leaf = leaf;
      leafp = trie_insert(rtrie, matrix, ++(*rdfa_st));
      leaf = leafp - (unsigned int *) rtrie->root;
      rdfa[1].trie_leaf = leaf;
   } else {
      // Compute DFA and RDFA.
      dfa = build_dfa(rows, nstat, keys, DFA_FORWARD);
      rdfa = build_dfa(rows, nstat, rkeys, DFA_REVERSE);
      // NULL tries.
      trie  = NULL;
      rtrie = NULL;
   }

   // ----- PROCESS FILE -----
   // Read and update
   int streak_dist = tau+1;
   int current_node = 1;
   long i = 0;
   unsigned long lines = 1;
   unsigned long linestart = 0;
   unsigned long count = 0;

   if (verbose) fprintf(stderr, "processing data\n");

   // DFA state.
   for (unsigned long k = 0; k < isize; k++, i++) {
      // Update DFA.
      state_t next  = dfa[current_node].next[(int)translate[(int)data[k]]];
      if (next.state == -1) next = build_dfa_step(rows, nstat, current_node, (int)data[k], &dfa, trie, keys, DFA_FORWARD);
      current_node = next.state;

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
               int rnode = 1;
               int d = tau + 1;
               // Find match start with RDFA.
               do {
                  int c = (int)data[linestart+i-++j];
                  state_t next = rdfa[rnode].next[(int)translate[c]];
                  if (next.state == -1) next = build_dfa_step(rows, nstat, rnode, c, &rdfa, rtrie, rkeys, DFA_REVERSE);
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
         current_node = 1;
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


state_t
build_dfa_step
(
 int        nstat,
 int        tau,
 int        dfa_state,
 int        base,
 dfa_t   ** dfap,
 btrie_t *  trie,
 char    *  exp,
 int        reverse
)
{
   // TODO: Base is already translated. Replace 'i' by 'base'.
   // int      i     = translate[base];
   // int      cols  = nstat/rows; WTF you need 'cols' now?
   dfa_t  * dfa   = *dfap;
   int    * state = (int *) *dfap; // Use dfa[0] as counter.
   int    * size  = state + 1;     // Use dfa[0] + 4B as size.
   int      value = 1 << base;
   int      maxval= tau + 1;

   // Reset matrix.
   int * newstate = calloc(nstat, sizeof(char));
   int   newcount  = 0;

   // Explore the trie backwards to find out the NFA state.
   // TODO: New trie_getpath with non-binary tree. (Reuse from futre versions).
   //       Save the value of the branch in the node to avoid further comparisons from the parent node.
   //       Values of the initial DFA state 0 are [tau+1 , tau , tau-1 , ... , 1 , 0 , 0 , ... , 0]
   int * curstate = trie_getpath(trie, dfa[dfa_state].trie_leaf);

   // Update NFA.
   for (int i = 0; i < nstat - 1; i++) {
      if ((exp[i] & value) > 0) newstate[i+1] = curstate[i];
      else {
         newstate[i] = max(newstate[i], curstate[i] - 1);
         // This max will avoid negative numbers.
         newstate[i+1] = max(newstate[i+1], curstate[i] - 1);
      }
      // Epsilon.
      if (i > 0 && newstate[i] < newstate[i-1] - 1) newstate[i] = newstate[i-1] - 1;
      newcount += newstate[i];
   }

   // Activate node zero (plus Epsilon).
   if (!reverse) {
      for (int i = 0; i < maxval; i++) {
         if (newstate[i] < maxval - i) newstate[i] = maxval - i;
         else break;
      }
   }

   if (newcount == 0) {
      dfa[dfa_state].next[i].match = tau + 1;
      dfa[dfa_state].next[i].state = 0;
      return dfa[dfa_state].next[i];
   }

   // Save match value.
   dfa[dfa_state].next[i].match = tau + 1 - newstate[nstat-1];;

   // Check if this state already exists.
   int exists = trie_search(trie, newstate);

   if (exists) {
      // If exists, just link with the existing state.
      dfa[dfa_state].next[i].state = exists;
   } else {
      // Insert new NFA state in the trie.
      unsigned int * leafp = trie_insert(trie, newstate, ++(*state));
      unsigned int leaf = leafp - (unsigned int *) trie->root; // bnode_t has 3 ints.
      // Create new state in DFA.
      if (*state >= *size) {
         *size *= 2;
         *dfap = dfa = realloc(dfa, *size * sizeof(dfa_t));
         if (dfa == NULL) {
            fprintf(stderr, "error (realloc) dfa in 'build_dfa_step': %s\n", strerror(errno));
            exit(1);
         }
         state = (int *) *dfap; // Use dfa[0] as counter.
         size  = state + 1;     // Use dfa[0] + 4B as size.
      }
      // Initialize DFA state as not computed. Annotate leaf.
      state_t new = {.state = DFA_COMPUTE, .match = rows};
      for (int j = 0; j < NBASES; j++) dfa[*state].next[j] = new;
      dfa[*state].trie_leaf = leaf;
      // Connect states.
      dfa[dfa_state].next[i].state = *state;
   }
   free(newmatrix);
   free(matrix);
   
   return dfa[dfa_state].next[i];
}

trie_t *
trie_new
(
 int initial_size,
 int height,
 int branch
 )
{

   // Allocate at least one node.
   if (initial_size < 1) initial_size = 1;
   if (height < 2) height = 2;
   if (branch < 2) branch = 2;

   trie_t * trie = malloc(sizeof(trie_t));
   if (trie == NULL) {
      fprintf(stderr, "error (malloc) trie_t in trie_new: %s\n", strerror(errno));
      exit(1);
   }
   trie->root = calloc(initial_size, sizeof(node_t) + branch * sizeof(node_t *));
   if (trie->root == NULL) {
      fprintf(stderr, "error (malloc) trie root in trie_new: %s\n", strerror(errno));
      exit(1);
   }
   trie->pos = 1;
   trie->size = initial_size;
   trie->height = height;
   trie->branch = branch;

   return trie;
}

unsigned int
trie_search
(
 trie_t * trie,
 int    * path
 )
{
   node_t * node = trie->root;

   for (int i = 0; i < trie->height; i++) {
      node_t * next = node->next[path[i]];
      if (next != NULL)  node = next;
      else return 0;
   }
   return 1;
}


int *
trie_getpath
(
 node_t * leaf,
 int height
)
{
   node_t * node = leaf;
   int    * path = malloc(height * sizeof(int));
   int      i    = height;
   

   // TODO: Is there a way to avoid the leaf to be a complete node?
   while (node != NULL && i > 0) {
      path[--i] = node->value;
      node = node->parent;
   } 

   // Control.
   if (i != 0) {
      free(path);
      return NULL;
   }

   return path;
}


node_t *
trie_insert
(
 trie_t       * trie,
 unsigned int * path,
)
{

   // If 'path' is  longer than trie height, the overflow is
   // ignored. 
   // If it is shorter, memory error may happen.

   node_t * node = trie->root;
   int i;
   for (i = 0; i < trie->height; i++) {
      if (path[i] >= branch) return NULL;
      // Walk the tree.
      if (node->next[path[i]] != NULL) {
         node = node->next[path[i]];
         continue;
      }
      
      // Node pointers and size.
      char * noderoot = (char *) trie->root;
      size_t nodesize = sizeof(node_t) + trie->branch * sizeof(node_t *);

      // Create new node.
      if (trie->pos >= trie->size) {
         size_t newsize = trie->size * 2;
         trie->root = realloc(trie->root, newsize * nodesize);
         if (trie->root == NULL) {
            fprintf(stderr, "error (realloc) in trie_insert: %s\n", strerror(errno));
            exit(1);
         }
         // Initialize memory.
         memset(noderoot + nodesize * trie->size, 0, trie->size * nodesize);
         trie->size = newsize;
      }
      
      // Consume one node of the trie.
      node_t * newnode = (node_t *) (noderoot + trie->pos * nodesize);
      node->next[path[i]] = newnode;
      newnode->value  = path[i];
      newnode->parent = node;
      trie->pos++;

      // Go one level deeper.
      node = newnode;
   }

   return node;
}


void
trie_reset
(
 trie_t * trie
 )
{
   free(trie->root);
   trie->pos = 0;
   trie->root = calloc(trie->size, sizeof(node_t) + trie->branch * sizeof(node_t *));
}


void
trie_free
(
 trie_t * trie
)
{
   free(trie->root);
   free(trie);
}
