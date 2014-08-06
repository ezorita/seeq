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
   int tau     = args->dist;
   unsigned long isize = args->isize;

   // Input stack mutex.
   pthread_mutex_t * stckinmutex = NULL;
   if (args->stckin != NULL) {
      stckinmutex = args->stckin[0]->mutex;
   }

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

   // Reverse the query and build reverse DFA.
   char * rkeys = malloc(wlen);
   for (int i = 0; i < wlen; i++)
      rkeys[i] = keys[wlen-i-1];

   dfa_t * dfa, * rdfa;
   trie_t * trie, * rtrie;
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
   trie  = trie_new(INITIAL_TRIE_SIZE, (cols-1)*sizeof(nfanode_t), ((int)1) << rows);
   rtrie = trie_new(INITIAL_TRIE_SIZE, (cols-1)*sizeof(nfanode_t), ((int)1) << rows);
   // Insert state 1.
   nfanode_t * matrix = calloc(cols - 1, sizeof(nfanode_t));
   matrix[0] = 1;
   epsilon(matrix, rows, cols);
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
   const int f_count = args->options & OPTION_COUNT;
   const int f_sline = args->options & OPTION_SHOWLINE;
   const int f_spos  = args->options & OPTION_SHOWPOS;
   const int f_sdist = args->options & OPTION_SHOWDIST;
   const int f_match = args->options & OPTION_MATCHONLY;
   const int f_umatch= args->options & OPTION_UNMATCHED;
   const int f_endl  = args->options & OPTION_ENDLINE;
   const int f_begl  = args->options & OPTION_BEGLINE;
   const int f_pline = args->options & OPTION_PRINTLINE;
   const int f_comp  = args->options & OPTION_COMPACT;
   const int f_matchoutput = f_sline + f_spos + f_sdist + f_match + f_endl + f_begl + f_pline + f_comp;

   // Process file
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
      int charin = (int)translate[(int)data[k]];
      state_t next;
      if (charin == 6) {
         while (data[k] != '\n' && k < isize) k++;
         // Reset line variables.
         linestart = k + 1;
         i = -1;
         lines++;
         current_node = 1;
         streak_dist = tau + 1;
         continue;
      } else if (charin == 5) {
         next.match = tau + 1;
      } else {
         next  = dfa[current_node].next[charin];
         if (next.state == -1) next = build_dfa_step(rows, nstat, current_node, charin, &dfa, trie, keys, DFA_FORWARD);
         current_node = next.state;
      }

      // Update streak.
      if (streak_dist > next.match) {
         // Tau is decreasing, track new streak.
         streak_dist   = next.match;
      } else if (streak_dist < next.match) { 
         while (data[k] != '\n' && k < isize) k++;
         count++;

         // FORMAT OUTPUT (quite crappy)
         if (!f_count && f_matchoutput) {
            data[k] = 0;
            long j = 0;
            if (args->options >= OPTION_RDFA) {
               int rnode = 1;
               int d = tau + 1;
               // Find match start with RDFA.
               do {
                  int c = (int)translate[(int)data[linestart+i-++j]];
                  state_t next = rdfa[rnode].next[c];
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
                  data[linestart + i] = 0;
                  off += sprintf(buffer+off, "%s", data+linestart+j);
               } else if (f_endl) {
                  off += sprintf(buffer+off, "%s", data+linestart+i);
               } else if (f_begl) {
                  data[linestart + j] = 0;
                  off += sprintf(buffer+off, "%s", data+linestart);
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

      if (charin == 5) {
         // If last line did not match, forward to next DFA.
         if (count == lastcount) {
            if (args->stckout != NULL) {
               filepos_t current = {.offset = linestart, .line = lines};
               push(args->stckout, current);
            }
            if (f_umatch) {
               data[k] = 0;
               fprintf(stdout, "%s\n", data+linestart);
               data[k] = '\n';
            }
         }
         lastcount = count;

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

   // Free expression keys.
   free(keys);
   free(rkeys);

   // Free DFAs.
   free(dfa);
   trie_free(trie);
   free(rdfa);
   trie_free(rtrie);

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
 trie_t  *  trie,
 char    *  exp,
 int        reverse
 )
{
   int      cols  = nstat/rows;
   dfa_t  * dfa   = *dfap;
   int    * state = (int *) *dfap; // Use dfa[0] as counter.
   int    * size  = state + 1;     // Use dfa[0] + 4B as size.
   int      value = 1 << base;
   int      mask  = (1 << rows) - 1;
   // Reset matrix.
   nfanode_t * newmatrix = calloc(cols, sizeof(nfanode_t));

   // Explore the trie backwards to find out the path (NFA status)
   nfanode_t * matrix = trie_getpath(trie, dfa[dfa_state].trie_leaf);

   for (int c = 0; c < cols-1; c++) {
      nfanode_t column = matrix[c];
      if ((column &= mask) == 0) continue;
      char      match  = exp[c] & value;
      // Match. Copy column.
      if (match) newmatrix[c + 1] |= column;
      // Mismatch. Shift column down.
      else {
         column <<= 1;
         newmatrix[c]     |= column;
         newmatrix[c + 1] |= column;
      }
   }

   // Activate node 0 in stream mode.
   if (reverse) {
      // Epsilon paths.
      nfanode_t active = 0;
      for (int c = 0; c < cols; c++) active |= matrix[c];
      if (!active) {
         dfa[dfa_state].next[base].match = rows;
         dfa[dfa_state].next[base].state = 0;
         return dfa[dfa_state].next[base];
      }
   } else {
      newmatrix[0] |= 1;
   }
   epsilon(newmatrix, rows, cols);

   // Check match.
   int       m = rows;
   nfanode_t matchcol = newmatrix[cols-1];
   if (matchcol) {
      m = 0;
      while (((matchcol >> m) & 1) == 0) m++;
   }
   // Save match value.
   dfa[dfa_state].next[base].match = m;

   // Check if this state already exists.
   int exists = trie_search(trie, newmatrix);

   if (exists) {
      // If exists, just link with the existing state.
      // No new jobs will be created.
      dfa[dfa_state].next[base].state = exists;
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
      dfa[dfa_state].next[base].state = *state;
   }
   free(newmatrix);
   free(matrix);

   return dfa[dfa_state].next[base];
}


void
epsilon
(
 nfanode_t * matrix,
 int rows,
 int cols
 )
{
   nfanode_t mask = ((unsigned int)1 << rows) - 1;
   // Epsilon.
   int i = 0;
   for (; i < cols - 1; i++) {
      matrix[i]   &= mask;
      matrix[i+1] |= (matrix[i] << 1);
   }
   matrix[i] &= mask;
}

trie_t *
trie_new
(
 int initial_size,
 int height,
 int branch
 )
{

   // Allocate at least one node with at least height 1.
   if (initial_size < 1) initial_size = 1;
   if (height < 2) height = 2;
   if (branch < 2) branch = 2;

   trie_t * trie = malloc(sizeof(trie_t));
   if (trie == NULL) {
      fprintf(stderr, "error (malloc) trie_t in trie_new: %s\n", strerror(errno));
      exit(1);
   }
   trie->root     = calloc(initial_size, sizeof(node_t) + branch * sizeof(unsigned int));
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
 char   * path
 )
{
   unsigned int   nodeid     = 0;
   unsigned int   nodeoffset = trie->branch + NODE_OVERHEAD;
   unsigned int * nodefield  = (unsigned int *) trie->root;

   for (int i = 0; i < trie->height; i++) {
      node_t * node = (node_t *) (nodefield + nodeoffset*nodeid);
      unsigned int next = node->next[(unsigned int)path[i]];
      if (next > 0) {
         nodeid = next;
      } else return 0;
   }
   return nodeid;
}


char *
trie_getpath
(
 trie_t       * trie,
 unsigned int   leaf
 )
{
   char * path  = malloc(trie->height);
   int    i     = trie->height;

   // Identify leaf.
   unsigned int * nodefield = (unsigned int *) trie->root;
   unsigned int nodeoffset = trie->branch + NODE_OVERHEAD;
   unsigned int current = leaf;
   path[--i] = leaf % nodeoffset - NODE_OVERHEAD;
   do {
      node_t * node = (node_t *) (nodefield + (current/nodeoffset) * nodeoffset);
      current  = node->parent;
      path[--i] = current % nodeoffset - NODE_OVERHEAD;
   } while (i > 0);

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
 trie_t       * trie,
 char         * path,
 unsigned int   value
 )
{

   // If 'path' is  longer then trie height, the overflow is
   // ignored. If it is shorter, memory error may happen.
   
   int i;

   unsigned int * nodefield  = (unsigned int *) trie->root;
   unsigned int   nodeoffset = trie->branch + NODE_OVERHEAD;

   // Start at node 0.
   unsigned int   current = 0;
   node_t       * node    = (node_t *) nodefield;

   for (i = 0; i < trie->height - 1; i++) {
      unsigned int next = node->next[(unsigned int)path[i]];
      if (next) {
         current = next;
         node = (node_t *) nodefield + current * nodeoffset;
         continue;
      }

      // Create new node.
      if (trie->pos >= trie->size) {
         size_t newsize = trie->size * 2;
         trie->root = realloc(trie->root, newsize*nodeoffset*sizeof(unsigned int));
         if (trie->root == NULL) {
            fprintf(stderr, "error (realloc) in trie_insert: %s\n",
                    strerror(errno));
            exit(1);
         }
         nodefield = (unsigned int *) trie->root;
         node = (node_t *) nodefield + current * nodeoffset;
         // Initialize memory.
         memset(nodefield + nodeoffset * trie->size, 0, trie->size * nodeoffset * sizeof(unsigned int));
         trie->size = newsize;
      }
      node->next[(int)path[i]] = trie->pos;
      node = (node_t *) (nodefield + trie->pos * nodeoffset);
      node->parent = current * nodeoffset + (int)path[i] + NODE_OVERHEAD;
      current = (trie->pos)++;
   }

   // Assign new value.
   unsigned int * data = nodefield + current * nodeoffset + (unsigned int)path[i] + NODE_OVERHEAD;
   *data = value;
   return data;
}


void
trie_reset
(
 trie_t * trie
 )
{
   free(trie->root);
   trie->pos = 0;
   trie->root = calloc(trie->size, (trie->branch + NODE_OVERHEAD) *sizeof(unsigned int));
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
