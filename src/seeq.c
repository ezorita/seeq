#include "seeq.h"

void *
seeq
(
 void * argsp
)
{
   //----- PARSE PARAMS -----
   struct seeqarg_t * args = (struct seeqarg_t *) argsp;

   char * data = args->data;
   char * expr = args->expr;
   int verbose = args->options & OPTION_VERBOSE;
   int tau     = args->dist;
   unsigned long isize = args->isize;

   // Input stack mutex.
   pthread_mutex_t * stckinmutex = NULL;
   if (args->stckin != NULL) {
      stckinmutex = args->stckin[0]->mutex;
   }

   if (verbose) fprintf(stderr, "parsing pattern\n");

   // ----- COMPUTE DFA -----
   char * keys;
   int    wlen = parse(expr, &keys);
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

   // ----- PROCESS FILE -----
   // Read and update
   int streak_dist = tau+1;
   int current_node = 1;
   long i = 0;
   unsigned long lines = 1;
   unsigned long linestart = 0;
   unsigned long count = 0;

   // Parse format options.
   int f_count = args->options & OPTION_COUNT;
   int f_sline = args->options & OPTION_SHOWLINE;
   int f_spos  = args->options & OPTION_SHOWPOS;
   int f_sdist = args->options & OPTION_SHOWDIST;
   int f_match = args->options & OPTION_MATCHONLY;
   int f_endl  = args->options & OPTION_ENDLINE;
   int f_pline = args->options & OPTION_PRINTLINE;
   int f_comp  = args->options & OPTION_COMPACT;

   if (verbose) fprintf(stderr, "processing data\n");

   long k = 0;
   long lastcount = 0;
   // Piped input.
   if (args->stckin != NULL) {
      pthread_mutex_lock(stckinmutex);
      filepos_t jump = pop(args->stckin);
      pthread_mutex_unlock(stckinmutex);
      // Update position.
      k = jump.offset;
      lines = jump.line;
      linestart = k;
      // Break if EOF flag is received.
      if (k == MSG_EOF) k = isize;
   }

   // DFA state.
   for (; k < isize; k++, i++) {

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
         count++;

         // FORMAT OUTPUT (quite crappy)
         if (!f_count) {
            data[k] = 0;
            long j = 0;
            if (args->options >= OPTION_RDFA) {
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
            if (f_comp) {
               fprintf(stdout, "%lu:%ld-%ld:%d\n",lines, j, i-1, streak_dist);
            } else {
               char buffer[STRLEN_POSITION+STRLEN_LINENO+STRLEN_DISTANCE+k-linestart+3];
               int  off = 0;
               if (f_sline) off += sprintf(buffer+off, "%lu ", lines);
               if (f_spos)  off += sprintf(buffer+off, "%ld-%ld ", j, i-1);
               if (f_sdist) off += sprintf(buffer+off, "%d ", streak_dist);
               if (f_match) {
                  data[linestart+i] = 0;
                  off += sprintf(buffer+off, "%s", data+linestart+j);
               } else if (f_endl) {
                  off += sprintf(buffer+off, "%s", data+linestart+i);
               } else if (f_pline) {
                  if(isatty(fileno(stdout)) && f_spos) {
                     char tmp = data[linestart + j];
                     data[linestart+j] = 0;
                     off += sprintf(buffer+off, "%s", data+linestart);
                     data[linestart+j] = tmp;
                     tmp = data[linestart+i];
                     data[linestart+i] = 0;
                     off += sprintf(buffer+off, (streak_dist > 0 ? BOLDRED : BOLDGREEN));
                     off += sprintf(buffer+off, "%s" RESET, data+linestart+j);
                     data[linestart+i] = tmp;
                     off += sprintf(buffer+off, "%s", data+linestart+i);
                  } else {
                     off += sprintf(buffer+off, "%s", data+linestart);
                  }
               }
               fprintf(stdout, "%s\n", buffer);
            }
            data[k] = '\n';
         }
      }

      if (data[k] == '\n') {
         // If last line did not match, forward to next DFA.
         if (args->stckout != NULL) {
            if (count == lastcount) {
               filepos_t current = {.offset = linestart, .line = lines};
               push(args->stckout, current);
            }
            lastcount = count;
         }

         // Piped input wait.
         if (args->stckin != NULL) {
            // Need to take mutex before dereferencing stckin to make it fully thread safe!
            pthread_mutex_lock(stckinmutex);
            filepos_t jump = pop(args->stckin);
            pthread_mutex_unlock(stckinmutex);
            // Update position.
            k = jump.offset;
            lines = jump.line - 1;
            // Break if EOF flag is received.
            if (k == MSG_EOF) break;
            k--;
         }

         // Reset line variables.
         linestart = k + 1;
         i = -1;
         lines++;
         current_node = 1;
         streak_dist = tau + 1;
      }

   }

   if (f_count) {
      if (args->options & OPTION_REVCOMP) {
         char * rev = reverse_pattern(args->expr);
         fprintf(stdout, "%s*\t%lu\n", rev, count);
         free(rev);
      } else {
         fprintf(stdout, "%s\t%lu\n", args->expr, count);
      }
   }
   
   // Flag EOF.
   if (args->stckout != NULL) seteof(args->stckout[0]);

   // Free DFA.
   free(rdfa);
   free(dfa);

   // Decrement num threads.
   pthread_mutex_lock(args->control->mutex);
   args->control->nthreads--;
   pthread_cond_signal(args->control->cond);
   pthread_mutex_unlock(args->control->mutex);

   // Free expression buffer.
   free(expr);
   // Free pipein.
   if (args->stckin != NULL) free(args->stckin[0]);
   // Free seeqargs.
   free(args);

   return NULL;
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
 int        rows,
 int        nstat,
 int        dfa_state,
 int        base,
 dfa_t   ** dfap,
 btrie_t *  trie,
 char    *  exp,
 int        reverse
)
{
   int      i     = translate[base];
   int      cols  = nstat/rows;
   dfa_t  * dfa   = *dfap;
   int    * state = (int *) *dfap; // Use dfa[0] as counter.
   int    * size  = state + 1;     // Use dfa[0] + 4B as size.
   int      value = 1 << i;

   // Reset matrix.
   char * newmatrix = calloc(nstat, sizeof(char));
   int    newcount  = 0;

   // Explore the trie backwards to find out the path (NFA status)
   char * matrix = trie_getpath(trie, dfa[dfa_state].trie_leaf);

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
      dfa[dfa_state].next[i].match = rows;
      dfa[dfa_state].next[i].state = 0;
      return dfa[dfa_state].next[i];
   }

   // Check match.
   int m = 0;
   int offset = nstat - rows;
   for (; m < rows; m++) if (newmatrix[offset + m]) break;

   // Save match value.
   dfa[dfa_state].next[i].match = m;

   // Check if this state already exists.
   int exists = trie_search(trie, newmatrix);

   if (exists) {
      // If exists, just link with the existing state.
      // No new jobs will be created.
      dfa[dfa_state].next[i].state = exists;
   } else {
      // Register new dfa state.
      unsigned int * leafp = trie_insert(trie, newmatrix, ++(*state));
      unsigned int leaf = leafp - (unsigned int *) trie->root; // bnode_t has 3 ints.
      // Create new state in DFA.
      if (*state >= *size) {
         *size *= 2;
         *dfap = dfa = realloc(dfa, *size * sizeof(dfa_t));
         if (dfa == NULL) {
            fprintf(stderr, "error (realloc) dfa in build_dfa: %s\n", strerror(errno));
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

char *
trie_getpath
(
  btrie_t      * trie,
  unsigned int   leaf
)
{
   bnode_t * nodes = trie->root;
   char * path = malloc(trie->height);
   int i      = trie->height;
   // Identify leaf.
   int node_ints = (sizeof(bnode_t)/sizeof(unsigned int));
   path[--i] = leaf % node_ints;
   int current;
   int parent = leaf / node_ints;
   do {
      current = parent;
      parent = nodes[current].parent;
      bnode_t  pnode  = nodes[parent];
      path[--i] = (pnode.next[0] == current ? 0 : 1);
   } while (parent != 0);

   // Control.
   if (i != 0) {
      free(path);
      return NULL;
   }

   return path;
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
      nodes[nodeid].next[(int)path[i]] = trie->pos;
      nodes[trie->pos].parent = nodeid;
      nodeid = (trie->pos)++;
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

sstack_t *
new_sstack
(
 int nelements
)
{
   // Allocate stack.
   sstack_t * sstack = malloc(sizeof(sstack_t) + nelements*sizeof(filepos_t));
   if (sstack == NULL) {
      fprintf(stderr, "error (malloc) sstack in 'new_sstack': %s\n", strerror(errno));
      exit(1);
   }
   sstack->p = 0;
   sstack->l = nelements;

   // Create monitor.
   pthread_mutex_t * mutex = malloc(sizeof(pthread_mutex_t));
   if (mutex == NULL) {
      fprintf(stderr, "error (malloc) mutex in 'new_sstack': %s\n", strerror(errno));
      exit(1);
   }
   pthread_mutex_init(mutex, NULL);
   sstack->mutex = mutex;
   
   pthread_cond_t * cond   = malloc(sizeof(pthread_cond_t));
   if (cond == NULL) {
      fprintf(stderr, "error (malloc) cond in 'new_sstack': %s\n", strerror(errno));
      exit(1);
   }
   pthread_cond_init(cond, NULL);
   sstack->cond = cond;

   sstack->eof = 0;

   return sstack;
}

void
push
(
 sstack_t  ** stackp,
 filepos_t    value
 )
{
   sstack_t * stack = *stackp;
   pthread_mutex_lock(stack->mutex);
   if (stack->p >= stack->l) {
      stack->l *= 2;
      *stackp = stack = realloc(stack, sizeof(sstack_t) + stack->l*sizeof(filepos_t));
      if (stack == NULL) {
         fprintf(stderr, "error (realloc) sstack_t in 'push': %s\n", strerror(errno));
         exit(1);
      }
   }

   stack->val[stack->p++] = value;
   pthread_cond_signal(stack->cond);
   pthread_mutex_unlock(stack->mutex);
}

filepos_t
pop
(
 sstack_t ** stackp
)
// This function must be called with the mutex LOCKED!
{
   sstack_t * stack = *stackp;

   while (stack->p == 0) {
      if (stack->eof == 1) {
         filepos_t eofflag = {.offset = MSG_EOF, .line = MSG_EOF};
         return eofflag;
      }
      pthread_cond_wait(stack->cond, stack->mutex);
      // Just in case a realloc happened while waiting.
      stack = *stackp;
   }
   filepos_t value = stack->val[--stack->p];

   return value;
}

void
seteof
(
 sstack_t * stack
)
{ 
   pthread_mutex_lock(stack->mutex);
   stack->eof = 1;
   pthread_cond_signal(stack->cond);
   pthread_mutex_unlock(stack->mutex);
}


char **
read_expr_file
(
 char * file,
 int  * nexpr
)
{
   FILE * input = fopen(file, "r");
   if (input == NULL) {
      fprintf(stderr, "error: could not open pattern file: %s\n", strerror(errno));
      exit(1);
   }
   
   *nexpr = 0;
   size_t  size = INITIAL_STACK_SIZE;
   char ** expr = malloc(size*sizeof(char *));
   char *  line = malloc(INITIAL_LINE_SIZE);

   if (expr == NULL || line == NULL) {
      fprintf(stderr, "error (malloc) in 'read_expr_file': %s\n", strerror(errno));
      exit(1);
   }
   
   ssize_t nread;
   while((nread = getline(&line, &size, input)) != -1) {
      if (line[nread-1] == '\n') line[nread-1] = 0;
      char * newexp = malloc(nread);
      strcpy(newexp, line);
      expr[(*nexpr)++] = newexp;
   }
   
   expr = realloc(expr, (*nexpr) * sizeof(char *));
   return expr;
}

char *
reverse_pattern
(
 char * expr
)
{
   int len = strlen(expr);
   char * reverse = malloc(len+1);
   for (int i = 0; i < len; i++) {
      reverse[i] = translate_reverse[(int)expr[len - 1 - i]];
      if (reverse[i] == 'X') {
         fprintf(stderr, "error: invalid pattern: %s\n", expr);
         exit(1);
      }
   }
   reverse[len] = 0;

   return reverse;
}
