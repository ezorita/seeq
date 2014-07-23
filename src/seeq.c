#include "seeq.h"

void
seeq
(
 char * expression,
 char * input,
 struct seeqarg_t args
)
{
   //----- PARSE PARAMS -----

   int tau = args.dist;
   if (input == NULL) {
      fprintf(stderr, "error: input file not specified.\n");
      exit(1);
   }

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
  
   // Initialize memory to compute DFA.
   dfa_t * dfa = malloc(INITIAL_DFA_SIZE*sizeof(dfa_t));
   btrie_t * trie = trie_new(INITIAL_TRIE_SIZE, nstat);
   int nstatus = 0;
   int dfasize = INITIAL_DFA_SIZE;
   nstack_t * start = new_stack(rows);

   // Compute DFA.
   build_dfa(rows, nstat, &nstatus, &dfasize, &dfa, start, keys, trie, DFA_FORWARD);
   trie_free(trie);

   // Reverse the query and build reverse DFA.
   char * rkeys = malloc(wlen);
   for (int i = 0; i < wlen; i++)
      rkeys[i] = keys[wlen-i-1];
   
   dfa_t * rdfa = malloc(INITIAL_DFA_SIZE*sizeof(dfa_t));
   trie = trie_new(INITIAL_TRIE_SIZE, nstat);
   int rstatus = 0;
   int rdfasize = INITIAL_DFA_SIZE;

   // Compute RDFA.
   build_dfa(rows, nstat, &rstatus, &rdfasize, &rdfa, start, rkeys, trie, DFA_REVERSE);
   free(start);   
   trie_free(trie);

   // ----- PROCESS FILE -----
   // Read and update
   int lines = 1;
   int i = 0;
   int post_match = 0;
   int streak_dist = tau+1, streak_pos = 0;
   int current_node = 0;
   int linestart = 0;
   int count = 0;

   // DFA status.
   for (unsigned long k = 0; k < isize; k++, i++) {
      if (data[k] == '\n') {
         linestart = k + 1;
         i = -1;
         lines++;
         post_match = streak_pos = current_node = 0;
         streak_dist = tau + 1;
         continue;
      }

      // Update DFA.
      status_t next  = dfa[current_node].next[(int)translate[(int)data[k]]];
      current_node = next.status;

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
                  int rnode = 0;
                  int d = tau + 1;
                  // Find match start with RDFA.
                  do {
                     status_t next = rdfa[rnode].next[(int)translate[(int)data[linestart+i-++j]]];
                     rnode = next.status;
                     d     = next.match;
                  } while (d > streak_dist && j < i);

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

int
build_dfa
(
 int         rows,
 int         nstat,
 int       * status,
 int       * size,
 dfa_t    ** dfap,
 nstack_t  * active,
 char      * exp,
 btrie_t   * trie,
 int         reverse
)
{
   dfa_t * dfa = *dfap;
   int empty_node = 1;
   int current_status = *status;
   int cols = nstat / rows;
   int mark = 0;
   char matrix[nstat];
   memset(matrix, 0, nstat);
   nstack_t * buffer = new_stack(INITIAL_STACK_SIZE);

   // Check if realloc is needed.
   if (current_status + 1 >= *size) {
      (*size) *= 2;
      *dfap = dfa = realloc(dfa, (*size) * sizeof(dfa_t));
      if (dfa == NULL) {
         fprintf(stderr, "error (realloc) dfa in build_dfa: %s\n", strerror(errno));
         exit(1);
      }
   }

   // Epsilon at node 0.
   int diag = 0;
   for (int i = 0; i < rows && !reverse; i++) {
      stack_add(&active, diag);
      diag += rows + 1;
   }

   // Compute all possible updates.
   for (int i = 0; i < NBASES; i++) {
      mark++;
      for (int j = 0; j < active->p; j++) {
         int node = active->val[j];
         // Hits are not updated. Continue then.
         if (node >= nstat - rows) continue;
         int value = 1 << i;
         // Match.
         if ((exp[node/rows] & value) > 0)
            setactive(rows, cols, node + rows, mark, matrix, &buffer);
         // Mismatch but not in the last row.
         else if (node % rows < rows - 1) {
            setactive(rows, cols, node + 1, mark, matrix, &buffer);
            setactive(rows, cols, node + rows + 1, mark, matrix, &buffer);
         }
      }
      if (buffer->p == 0) {
         dfa[current_status].next[i].match = rows;
         dfa[current_status].next[i].status = 0;
         continue;
      }
      empty_node = 0;

      // Check match.
      int m = 0;
      int offset = nstat - rows;
      for (; m < rows; m++) if (matrix[offset + m] == mark) break;

      // Save match value.
      dfa[current_status].next[i].match = m;

      // Check if this status already exists.
      char path[nstat];
      memset(path, 0, nstat);
      for (int k = 0; k < buffer->p; k++) path[buffer->val[k]] = 1;
      int exists = trie_search(trie, path);

      if (exists) {
         // If exists, redirect to the matching status and leave this branch.
         dfa[current_status].next[i].status = exists;
      } else {
         // Save pointer to the next status.
         dfa[current_status].next[i].status = ++(*status);
         // Insert status into trie.
         trie_insert(trie, path, (*status));
         // Continue building DFA starting at status.
         int sub_empty = build_dfa(rows, nstat, status, size, dfap, buffer, exp, trie, reverse);
         // Re-read dfa address (in case realloc happened).
         dfa = *dfap;
         // Check whether the child node is empty.
         if (sub_empty) {
            // Next node is empty. Return node to DFA and set next and trie to 0.
            // Can do status-- because we haven't created any new node!
            (*status)--;
            dfa[current_status].next[i].status = 0;
            trie_insert(trie, path, 0);
         }
      }

      // Reset buffer
      buffer->p = 0;
   }
   free(buffer);

   return empty_node;
}


btrie_t *
trie_new
(
 int initial_size,
 int height
)
{
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


void
trie_insert
(
 btrie_t * trie,
 char    * path,
 int       value
)
// Returns 0 if inserted node was not already in the trie, 1 otherwise.
{
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
            fprintf(stderr, "error (realloc) in trie_insert: %s\n", strerror(errno));
            exit(1);
         }
         // Initialize memory.
         bnode_t zero;
         zero.next[0] = 0;
         zero.next[1] = 0;

         for (int k = trie->pos; k < trie->size; k++) trie->root[k] = zero;
      }

      nodeid = nodes[nodeid].next[(int)path[i]] = (trie->pos)++;
   }

   // Assign new value.
   nodes[nodeid].next[(int)path[i]] = value;
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

void
setactive
(
 int         rows,
 int         cols,
 int         node,
 int         status,
 char      * matrix,
 nstack_t ** buffer

)
{
   if (node >= rows*cols) return;
   if (matrix[node] == status) return;
   matrix[node] = status;
   stack_add(buffer, node);
   // Last row line, return.
   if (node % rows == rows - 1) return;
   // Epsilon.
   int free = node + rows + 1;
   if (free < rows * cols) setactive(rows, cols, free, status, matrix, buffer);
}


nstack_t *
new_stack
(
 int elements
)
{
   if (elements < 1) elements = 1;
   nstack_t * stack = malloc(sizeof(nstack_t) + elements*sizeof(int));
   if (stack == NULL) {
      fprintf(stderr, "error (malloc) nstack_t in new_stack: %s\n", strerror(errno));
      exit(1);
   }
   stack->p = 0;
   stack->l = elements;
   return stack;
}

void
stack_add
(
 nstack_t ** stackp,
 int         value
)
{
   nstack_t * stack = *stackp;
   if (stack->p >= stack->l) {
      size_t newsize = 2*stack->l;
      *stackp = stack = realloc(stack, sizeof(nstack_t)+newsize*sizeof(int));
      if (stack == NULL) {
         fprintf(stderr, "error (realloc) nstack_t in stack_add: %s\n", strerror(errno));
         exit(1);
      }
      stack->l = newsize;
   }

   stack->val[stack->p++] = value;
}

