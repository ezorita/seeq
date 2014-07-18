#include "seeq.h"

void SIGSEGV_handler(int sig) {
   void *array[10];
   size_t size;

   // get void*'s for all entries on the stack
   size = backtrace(array, 10);

   // print out all the frames to stderr
   fprintf(stderr, "Error: signal %d:\n", sig);
   backtrace_symbols_fd(array, size, 2);
   exit(1);
}



int main(int argc, char *argv[])
{
   signal(SIGSEGV, SIGSEGV_handler); 

   if (argc != 4) {
      fprintf(stderr,"usage: seeq expression distance inputfile\n");
      exit(0);
   }

   char * expression = argv[1];
   int    tau = atoi(argv[2]);
   FILE * in = fopen(argv[3], "r");
   
   if (in == NULL) {
      fprintf(stderr,"error: could not open file\n");
      exit(1);
   }
   
   char * keys;
   int    wlen = parse(expression, &keys);
   int    nnfa = 2*tau + 1;
   int    rows = tau + 1;
   int    cols = wlen + 1;
   int    nstat= cols*rows;
   
   if (rows >= cols) {
      fprintf(stderr, "error: expression must be longer than the maximum distance.\n");
      exit(1);
   }

   nstack_t * start = new_stack(rows);
   int diag = 0;
   // Compute epsilon startpoint.
   for (int i = 0; i < rows; i++) {
      stack_add(&start, diag);
      diag += rows + 1;
   }
   
   // Maximum number of states is 2^(tau+1)*wlen.
   dfa_t * dfa = calloc(((int)(1) << (tau + 1)) * wlen, sizeof(dfa_t));
   btrie_t * trie = trie_new(INITIAL_TRIE_SIZE, nstat);
   int nstatus = 0;

   // Compute DFA.
   build_dfa(rows, nstat, &nstatus, dfa, start, keys, trie);

   free(start);
   trie_free(trie);

   // DEBUG DFA:
   for (int i = 0; i <= nstatus; i++) {
      fprintf(stdout, "[node %d]\n", i);
      for (int j = 0; j < NBASES; j++) {
         fprintf(stdout, " %c:\t%d\t(%d)\n", bases[j], dfa[i].next[j].status, dfa[i].next[j].match);
      }
      fprintf(stdout, "\n");
   }
   exit(0);

   // Read and update
   char * line = malloc(INITIAL_LINE_SIZE*sizeof(char));
   size_t bsize = INITIAL_LINE_SIZE;
   ssize_t size;
   int lines = 0;

   // DFA status.
   pstack_t * active = new_pstack(rows + cols);
   pstack_t * next   = new_pstack(rows + cols);
   status_t startnode = {.status = 0, .match = tau + 1};

   while ((size = getline(&line, &bsize, in)) != -1) {
      lines++;
      line[size] = 0;
      if (size < wlen - tau) continue;
      int post_match = 0;
      int start_dist = tau+1, start_pos = 0, start_offset = tau + 1;
      int dist = tau + 1, offset = tau + 1;
      for (int i = 0; i < size; i++) {
         // Add a new path to the DFA.
         path_t new = {.node = startnode, .start = i};
         pstack_add(active, new);

         // Update DFA
         // Each new character follows an independent path in only one DFA.
         // This is like a car race, where the input characters are the cars
         // and they all run in the same circuit (dfa).
         match_t match = update(line[i], active, next, dfa, i, tau, wlen);

         // Swap path buffers.
         pstack_t * temp = active;
         active = next;
         next = temp;

         // Compute matches
         dist = match.dist;
         offset = i - match.start - wlen;

         // TODO: RENAME TO STREAK_START, DIST IS NOW TAKEN FROM MATCH
         if (start_dist < dist) {
            if (post_match == 1) {
               start_pos    = i;
               start_dist   = dist;
               start_offset = offset;
            } else {
               fprintf(stdout, "%d,%d:%d+%d (%d)\n", lines, start_pos-wlen+1-start_offset, start_pos, i-1-start_pos, start_dist);
               // break if only first match is desired
               //break;
               post_match = 1;
               start_offset = tau + 1;
            }
         } else if (start_dist == dist && dist <= tau) {
            if (start_offset*start_offset > offset*offset) {
               start_pos    = i;
               start_offset = offset;
            }
            post_match = 0;
         } else {
            start_pos    = i;
            start_dist   = dist;
            start_offset = offset;
            post_match   = 0;
         }
         //printf(stdout, "start_i = %d\tstart_d = %d\tstart_o = %d\n", start_pos, start_dist, start_offset);
      }

      // Report last match if start_dist == dist.
      if (start_dist == dist && dist <= tau) {
         // TODO.
      }

      // Clear active nodes.
      active->p = 0;
      next->p = 0;
   }

   return 0;
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


match_t
update
(
 char       value,
 pstack_t * active,
 pstack_t * next,
 dfa_t    * dfa,
 int        pos,
 int        tau,
 int        wlen
)
{
   int mindist = tau + 1;
   int minoffs = tau + 1;
   match_t match = {tau + 1, 0};
   value = translate[value];

   // Clear next.
   next->p = 0;

   // Update DFA paths.
   for (int i = 0; i < active->p; i++) {
      path_t path = active->path[i];
      path_t new;
      int  status = path.node.status;
      new.node  = dfa[status].next[value];
      new.start = path.start;
      if(new.node.status) {
         pstack_add(next, new);
         int dist = new.node.match;
         int offs = pos - new.start - wlen;
         if (dist < mindist) {
            mindist = dist;
            minoffs = offs;
            match.dist  = dist;
            match.start = new.start;
         } else if (dist == mindist && offs*offs < minoffs*minoffs) {
            minoffs = offs;
            match.dist  = dist;
            match.start = new.start;
         }
      }
   }

   return match;
}

int
build_dfa
(
 int         rows,
 int         nstat,
 int       * status,
 dfa_t     * dfa,
 nstack_t  * active,
 char      * exp,
 btrie_t   * trie
)
// dfa_t must be initialized to 0! (so use calloc before passing)
{
   int empty_node = 1;
   int current_status = *status;
   int cols = nstat / rows;
   char matrix[nstat];
   for (int i = 0; i < nstat; i++) matrix[i] = 0;
   nstack_t * buffer = new_stack(INITIAL_STACK_SIZE);

   for (int i = 0; i < NBASES; i++) {
      int mark = i + 1;
      for (int j = 0; j < active->p; j++) {
         int node = active->val[j];
         // If it's a hit, set inactive.
         if (node > nstat - rows) continue;
         int value = 1 << i;
         if ((exp[node/rows] & value) > 0)
            setactive(rows, cols, node + rows, mark, matrix, &buffer);
         else if (node % rows < rows - 1) {
            setactive(rows, cols, node + 1, mark, matrix, &buffer);
            setactive(rows, cols, node + rows + 1, mark, matrix, &buffer);
         }
      }
      if (buffer->p == 0) {
         dfa[current_status].next[i].match = rows;
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
      for (int k = 0; k < nstat; k++) path[k] = 0;
      for (int k = 0; k < buffer->p; k++) path[buffer->val[k]] = 1;
      int exists = trie_insert(trie, path, (*status) + 1);

      if (exists) {
         // If exists, redirect to the matching status and leave this branch.
         dfa[current_status].next[i].status = exists;
      } else {
         // Save pointer to the next status.
         dfa[current_status].next[i].status = ++(*status);
         // Continue building DFA starting at status.
         if (build_dfa(rows, nstat, status, dfa, buffer, exp, trie)) {
            // Next node was empty. Return node to DFA and set next to 0.
            // Can do status-- because we haven't created any new node!
            (*status)--;
            dfa[current_status].next[i].status = 0;
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
      fprintf(stderr, "error (malloc): btrie_t\n");
      exit(1);
   }
   trie->root     = calloc(initial_size, sizeof(bnode_t));
   if (trie->root == NULL) {
      fprintf(stderr, "error (malloc): trie root\n");
      exit(1);
   }
   trie->pos = 1;
   trie->size = initial_size;
   trie->height = height;

   return trie;
}

int
trie_insert
(
 btrie_t * trie,
 char    * path,
 int       value
)
{
   int i;
   int found  = 0;
   int nodeid = 0;

   bnode_t * nodes = trie->root;

   for (i = 0; i < trie->height - 1; i++) {
      int next = nodes[nodeid].next[path[i]];
      if (next > 0) {
         nodeid = next;
         continue;
      }

      found = 1;
      
      // Create new node.
      if (trie->pos >= trie->size) {
         trie->root = nodes = realloc(nodes, 2*trie->size*sizeof(bnode_t));
         if (nodes == NULL) {
            fprintf(stderr, "error (realloc): trie_insert\n");
            exit(1);
         }
         trie->size *= 2;
      }

      nodeid = nodes[nodeid].next[path[i]] = trie->pos++;
   }
   int final = nodes[nodeid].next[path[i]];
   // If the value is not present, insert it and return 0.
   if (!found || !final) {
      nodes[nodeid].next[path[i]] = value;
      return 0;
   } else return final;
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
   if (matrix[node] == status || node >= rows*cols) return;
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
   nstack_t * stack = malloc(sizeof(nstack_t) + elements*sizeof(int));
   if (stack == NULL) {
      fprintf(stderr, "error allocating nstack_t\n");
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
         fprintf(stderr, "error reallocating nstack\n");
         exit(1);
      }
   }
   
   stack->val[stack->p++] = value;
}


pstack_t *
new_pstack
(
 int elements
)
{
   pstack_t * stack = malloc(sizeof(pstack_t) + elements*sizeof(path_t));
   if (stack == NULL) {
      fprintf(stderr, "error allocating pstack_t\n");
      exit(1);
   }
   stack->p = 0;
   return stack;
}

void
pstack_add
(
 pstack_t * stack,
 path_t     item
 )
{
   stack->path[stack->p++] = item;
}
