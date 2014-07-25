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
   
   if (rows >= cols) {
      fprintf(stderr, "error: expression must be longer than the maximum distance.\n");
      exit(1);
   }

   if (verbose) fprintf(stderr, "building DFA\n");

   // Compute DFA.
   dfa_t * dfa = build_dfa(rows, nstat, keys, DFA_FORWARD);
   
   // Reverse the query and build reverse DFA.
   char * rkeys = malloc(wlen);
   for (int i = 0; i < wlen; i++)
      rkeys[i] = keys[wlen-i-1];

   //Compute RDFA.
   dfa_t * rdfa = build_dfa(rows, nstat, rkeys, DFA_REVERSE);
   free(rkeys);

   // ----- PROCESS FILE -----
   // Read and update
   int lines = 1;
   int i = 0;
   int post_match = 0;
   int streak_dist = tau+1, streak_pos = 0;
   int current_node = 1;
   int linestart = 0;
   int count = 0;

   if (verbose) fprintf(stderr, "processing data\n");

   // DFA state.
   for (unsigned long k = 0; k < isize; k++, i++) {
      if (data[k] == '\n') {
         linestart = k + 1;
         i = -1;
         lines++;
         post_match = streak_pos = 0;
         current_node = 1;
         streak_dist = tau + 1;
         continue;
      }

      // Update DFA.
      state_t next  = dfa[current_node].next[(int)translate[(int)data[k]]];
      current_node = next.state;

      // Update streak.
      if (streak_dist == next.match) continue;
      else if (streak_dist > next.match) {
         // Tau is decreasing, track new streak.
         streak_pos    = i;
         streak_dist   = next.match;
         post_match    = 0;
      } else { 
         if (post_match == 1) {
            // Coming from a recent match, just update.
            streak_pos    = i;
            streak_dist   = next.match;
         } else {
            // Format only if stdout == default stdout!
            while (data[k] != '\n' && k < isize) k++;
            data[k] = 0;

            // FORMAT OUTPUT (quite crappy)
            if (args.count) {
               count++;
            } else {
               int j = 0;
               if (args.showpos || args.compact || args.matchonly) {
                  int rnode = 1;
                  int d = tau + 1;
                  // Find match start with RDFA.
                  do {
                     state_t next = rdfa[rnode].next[(int)translate[(int)data[linestart+i-++j]]];
                     rnode = next.state;
                     d     = next.match;
                  } while (rnode && d > streak_dist && j < i);

                  // Compute match length and print match.
                  j = i - j;
               }
               if (args.compact) {
                  fprintf(stdout, "%d:%d-%d:%d\n",lines, j, i-1, streak_dist);
               } else {
                  if (args.showline) fprintf(stdout, "[%d] ", lines);
                  if (args.showpos)  fprintf(stdout, "%d-%d ", j, i-1);
                  if (args.showdist) fprintf(stdout, "%d ", streak_dist);
                  if (args.matchonly) {
                     data[linestart+i] = 0;
                     fprintf(stdout, "%s", data+linestart+j);
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
            data[k--] = '\n';
            continue;
            // Filter flag.
            post_match = 1;
         }
      }
   }

   if (args.count) fprintf(stdout, "%d\n", count);

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

      if (!add) l++;
      i++;
   }
   
   if (add == 1) return -1;
   else return l;
}

dfa_t *
build_dfa
(
 int         rows,
 int         nstat,
 char      * exp,
 int         reverse
)
{
   // Columns.
   int cols = nstat / rows;

   // Create DFA and trie.
   btrie_t * trie = trie_new(INITIAL_TRIE_SIZE, nstat - rows);
   dfa_t   * dfa  = malloc(INITIAL_DFA_SIZE*sizeof(dfa_t));

   // DFA control vars.
   int state = 0;
   int size  = INITIAL_DFA_SIZE;
   
   // Initialize job stack.
   jstack_t * stack = new_jstack(INITIAL_STACK_SIZE);

   // Activate initial state.
   char * startmatrix = calloc(nstat, sizeof(char));
   setactive(rows, cols, 0, startmatrix);
   // Add initial job.
   job_t start = {.nfa_state = startmatrix, .link = 0};
   push(&stack, start);

   while(stack->p > 0) {
      // Pop new job from stack.
      job_t job = pop(stack);

      // Check if realloc is needed before dereferencing.
      if (state + 1 >= size) {
         size *= 2;
         dfa = realloc(dfa, size * sizeof(dfa_t));
         if (dfa == NULL) {
            fprintf(stderr, "error (realloc) dfa in build_dfa: %s\n", strerror(errno));
            exit(1);
         }
      }

      // Take job parameters.
      char * matrix = job.nfa_state;
      int  * link   = ((int *) dfa) + job.link;

      // If I already exist, send my parent somewhere else.
      int exists = trie_search(trie, matrix);
      if (exists) {
         *link = exists;
         continue;
      }

      // Let's register this node before we start.
      // If we find that is empty we'll return it to
      // the DFA before finishing the job.
      state++;

      // Compute all possible updates.
      int empty_node = 1;
      for (int i = 0; i < NBASES; i++) {
         // Reset matrix.
         char * newmatrix = calloc(nstat, sizeof(char));
         int    newcount  = 0;
         int    value     = 1 << i;

         for (int j = 0; j < nstat - rows; j++) {
            // Not active, continue.
            if (!matrix[j]) continue;
            // Match.
            if ((exp[j/rows] & value) > 0)
               newcount += setactive(rows, cols, j + rows, newmatrix);
            // Mismatch but not in the last row.
            else if (j % rows < rows - 1) {
               newcount += setactive(rows, cols, j + 1, newmatrix);
               newcount += setactive(rows, cols, j + rows + 1, newmatrix);
            }
         }

         // Epsilon at node 0.
         if (!reverse) newcount += setactive(rows, cols, 0, newmatrix);

         if (newcount == 0) {
            dfa[state].next[i].match = rows;
            dfa[state].next[i].state = 0;
            continue;
         }

         // The node is not empty, now we can link to parent
         // and add to trie.
         if (empty_node) {
            empty_node = 0;
            *link = state;
            trie_insert(trie, matrix, state);
         }

         // Check match.
         int m = 0;
         int offset = nstat - rows;
         for (; m < rows; m++) if (newmatrix[offset + m]) break;

         // Save match value.
         dfa[state].next[i].match = m;

         // Check if this state already exists.
         exists = trie_search(trie, newmatrix);

         if (exists) {
            // If exists, just link with the existing state.
            // No new jobs will be created.
            dfa[state].next[i].state = exists;
         } else {
            // Create job and push to stack.
            int offset   = (int *)&(dfa[state].next[i].state) - (int *)dfa;
            job_t newjob = {.nfa_state = newmatrix, .link = offset};
            push(&stack, newjob);
         }
      }
   
      if (empty_node) {
         // Unregister node if empty.
         *link = 0;
         state--;
      }
      free(matrix);
   }
   
   // Free stack and trie.
   trie_free(trie);
   free(stack);

   // Resize DFA and return.
   dfa = realloc(dfa, (state+1)*sizeof(dfa_t));
   return dfa;
}


btrie_t *
trie_new
(
 int initial_size,
 int height
)
{

   // Allocate at least one node with at least height 1.
   if (initial_size < 1) initial_size = 1;
   if (height < 1) height = 1;

   btrie_t * trie = malloc(sizeof(btrie_t));
   if (trie == NULL) {
      fprintf(stderr, "error (malloc) btrie_t in trie_new: %s\n", strerror(errno));
      exit(1);
   }
   trie->root     = calloc(initial_size, sizeof(bnode_t));
   if (trie->root == NULL) {
      fprintf(stderr, "error (malloc) trie root in trie_new: %s\n", strerror(errno));
      exit(1);
   }
   trie->pos = 1;
   trie->size = initial_size;
   trie->height = height;

   return trie;

}


int
trie_search
(
 btrie_t * trie,
 char    * path
)
{
   int nodeid = 0;
   bnode_t * nodes = trie->root;

   for (int i = 0; i < trie->height; i++) {
      int next = nodes[nodeid].next[(int)path[i]];
      if (next > 0) {
         nodeid = next;
      } else return 0;
   }
   return nodeid;
}


unsigned int *
trie_insert
(
 btrie_t      * trie,
 char         * path,
 unsigned int   value
)
{

   // If 'path' is  longer then trie height, the overflow is
   // ignored. If it is shorter, memory error may happen.
   
   int i;
   int nodeid = 0;

   bnode_t * nodes = trie->root;

   for (i = 0; i < trie->height - 1; i++) {
      int next = nodes[nodeid].next[(int)path[i]];
      if (next > 0) {
         nodeid = next;
         continue;
      }

      // Create new node.
      if (trie->pos >= trie->size) {
         trie->size *= 2;
         trie->root = nodes = realloc(nodes, trie->size*sizeof(bnode_t));
         if (nodes == NULL) {
            fprintf(stderr, "error (realloc) in trie_insert: %s\n",
                  strerror(errno));
            exit(1);
         }
         // Initialize memory.
         bnode_t zero = { .next = {0,0} };
         for (int k = trie->pos; k < trie->size; k++) trie->root[k] = zero;
      }
      nodeid = nodes[nodeid].next[(int)path[i]] = (trie->pos)++;
   }

   // Assign new value.
   unsigned int * data = &(nodes[nodeid].next[(int)path[i]]);
   *data = value;
   return data;
}


void
trie_reset
(
 btrie_t * trie
)
{
   free(trie->root);
   trie->pos = 0;
   trie->root = calloc(trie->size, sizeof(bnode_t));
}


void
trie_free
(
 btrie_t * trie
)
{
   free(trie->root);
   free(trie);
}

int
setactive
(
 int         rows,
 int         cols,
 int         node,
 char      * matrix
)
{
   if (node >= rows*cols) return 0;
   if (matrix[node]) return 0;
   matrix[node] = 1;
   // Last row line, return.
   if (node % rows == rows - 1) return 1;
   // Epsilon.
   int free = node + rows + 1;
   if (free < rows * cols) return 1 + setactive(rows, cols, free, matrix);
   else return 1;
}

jstack_t *
new_jstack
(
 int elements
)
{
   if (elements < 1) elements = 1;
   jstack_t * stack = malloc(sizeof(jstack_t) + elements*sizeof(job_t));
   if (stack == NULL) {
      fprintf(stderr, "error (malloc) jstack_t in new_jstack: %s\n", strerror(errno));
      exit(1);
   }
   stack->p = 0;
   stack->l = elements;
   return stack;
}

void
push
(
 jstack_t ** stackp,
 job_t       job
)
{
   jstack_t * stack = *stackp;
   if (stack->p >= stack->l) {
      size_t newsize = 2*stack->l;
      *stackp = stack = realloc(stack, sizeof(jstack_t)+newsize*sizeof(job_t));
      if (stack == NULL) {
         fprintf(stderr, "error (realloc) jstack_t in push: %s\n", strerror(errno));
         exit(1);
      }
      stack->l = newsize;
   }

   stack->job[stack->p++] = job;
}

job_t
pop
(
 jstack_t * stack
)
{
   return stack->job[--(stack->p)];
}
