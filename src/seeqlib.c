struct seeq_t {
   int     tau;
   int     wlen;
   long    line;
   long    count;
   char  * keys;
   char  * rkeys;
   dfa_t * dfa;
   dfa_t * rdfa;
   FILE  * fdi;
};

seeq_t *
seeqOpen
(
 char * file,
 char * pattern,
 int    mismatches
)
{
   // Check parameters.
   if (mismatches < 0) return NULL;

   // Parse pattern
   char *  keys = malloc(strlen(pattern));
   if (keys == NULL) return NULL;

   char * rkeys = malloc(strlen(pattern));
   if (rkeys == NULL) {
      free(keys);
      return NULL;
   }

   int wlen = parse(pattern, keys);
   if (wlen == -1) {
      free(keys); free(rkeys);
      return NULL;
   }
   for (int i = 0; i < wlen; i++) rkeys[i] = keys[wlen-i-1];

   // Check parameters.
   if (mismatches >= wlen) {
      free(keys); free(rkeys);
      return NULL;
   }

   // Allocate DFAs.
   dfa_t * dfa = dfa_new(wlen, mismatches, INITIAL_DFA_SIZE, INITIAL_TRIE_SIZE);
   if (dfa == NULL) {
      free(keys); free(rkeys);
      return NULL;
   }

   dfa_t * rdfa = dfa_new(wlen, mismatches, INITIAL_DFA_SIZE, INITIAL_TRIE_SIZE);
   if (dfa == NULL) {
      free(keys); free(rkeys); free(dfa);
      return NULL;
   }

   seeq_t * sqfile = malloc(sizeof(seeq_t));
   if (sqfile == NULL) {
      free(keys); free(rkeys); free(dfa); free(rdfa);
      return NULL;
   }

   // Open file or set to stdin
   FILE * fdi = stdin;
   if (file != NULL) fdi = fopen(file, "r");
   if (fdi  == NULL) {
      free(keys); free(rkeys); free(dfa); free(rdfa); free(sqfile);
      return NULL;
   }

   // Set seeq_t struct.
   sqfile->tau   = mismatches;
   sqfile->wlen  = wlen;
   sqfile->line  = 0;
   sqfile->count = 0;
   sqfile->keys  = keys;
   sqfile->rkeys = rkeys;
   sqfile->dfa   = dfa;
   sqfile->rdfa  = rdfa;
   sqfile->fdi   = fdi;

   return sqfile;
}

long
seeqMatch
(
 sqmatch_t * match,
 seeq_t    * sqfile,
 int         options
)
{
   // Count replaces all other options.
   if (options & SQ_COUNT) options = 0;
   else if (options == 0) options = SQ_MATCH | SQ_NOMATCH;

   // Set structure to non-matched.
   match->start = -1;

   // Aux vars.
   long startline = sqfile->line;
   size_t bufsz = INITIAL_LINE_SIZE;
   ssize_t readsz;
   long count = 0;

   while ((readsz = getline(&(match->match), &bufsz, fdi)) > 0) {
      char * data = match->match;
      // Remove newline.
      if (data[readsz-1] == '\n') data[readsz---1] = 0;

      // Reset search variables
      int streak_dist = sqfile->tau+1;
      int current_node = 0;
      sqfile->line++;

      // DFA state.
      for (unsigned long i = 0; i <= readsz; i++) {
         // Update DFA.
         int cin = (int)translate[(int)data[i]];
         edge_t next;
         if(cin < NBASES) {
            next = dfa->states[current_node].next[cin];
            if (next.match == DFA_COMPUTE)
               if (dfa_step(current_node, cin, sqfile->wlen, sqfile->tau, &(sqfile->dfa), sqfile->keys, &next)) return -1;
            current_node = next.state;
         } else if (cin == 5) {
            next.match = sqfile->tau+1;
            if ((options & SQ_NOMATCH) && streak_dist >= next.match) {
               match->line  = sqfile->line;
               match->match = data;
               match->start = 0;
               match->end   = readsz;
               match->dist  = -1;
               break;
            }
         } else {
            // Now lines containing illegal characters will be ommited.
            break;
            // Consider changing this by:
            //   streak_dist = tau + 1;
            //   current_node = 0;
            //   continue;
            // to report these lines as non-matches.
         }

         // Update streak.
         if (streak_dist > next.match) {
            // Tau is decreasing, track new streak.
            streak_dist   = next.match;
         } else if (streak_dist < next.match) {
            count++;
            if (options & SQ_MATCH) {
               long j = 0;
               int rnode = 0;
               int d = sqfile->tau + 1;
               // Find match start with RDFA.
               do {
                  int c = (int)translate[(int)data[i-++j]];
                  edge_t next = rdfa->states[rnode].next[c];
                  if (next.match == DFA_COMPUTE)
                     if (dfa_step(rnode, c, sqfile->wlen, sqfile->tau, &(sqfile->rdfa), sqfile->rkeys, &next)) return -1;
                  rnode = next.state;
                  d     = next.match;
               } while (d > streak_dist && j < i);

               // Compute match length and print match.
               j = i - j;

               match->start = j;
               match->end   = i;
               match->line  = sqfile->line;
               match->dist  = streak_dist;
            }
            break;
         }
      }
      if (match->start >= 0) break;
   }

   // If nothing was read, return -1.
   if (sqfile->line == startline) return -1;
   else return count;
}
