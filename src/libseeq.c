/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
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

#include "libseeq.h"
#include "seeqcore.h"

int seeqerr = 0;

static const char *
seeq_strerror[12] =
   {"Check errno",
    "Illegal matching distance value",
    "Incorrect pattern (double opening brackets)",
    "Incorrect pattern (double closing brackets)",
    "Incorrect pattern (illegal character)",
    "Incorrect pattern (missing closing bracket)",
    "Illegal path value passed to 'trie_search'",
    "Illegal nodeid passed to 'trie_getrow' (node is not a leaf).\n",
    "Illegal path value passed to 'trie_insert'",
    "Pattern length must be larger than matching distance",
    "Passed seeq_t struct does not contain a valid file pointer",
    "End of line reached."};

seeq_t *
seeqNew
(
 const char * pattern,
 int          mismatches,
 size_t       maxmemory
)
// SYNOPSIS:                                                              
//   Creates a new seeq_t structure for the defined pattern and matching distance.
//   An empty DFA network is created and stored internally. The DFA network grows
//   each time this structure is passed to a matching function.
//                                                                        
// PARAMETERS:                                                            
//   pattern    : matching pattern (accepted characters 'A','C','G','T','U','N','[',']').
//   mismatches : matching distance (Levenshtein distance).
//   maxmemory  : DFA memory limit, in bytes.
//
// RETURN:                                                                
//   Returns a pointer to a seeq_t structure or NULL in case of error, and seeqerr is
//   set appropriately.
//
// SIDE EFFECTS:
//   The returned seeq_t structure must be freed using 'seeqFree'.
{

   // Check parameters.
   if (mismatches < 0) {
      seeqerr = 1;
      return NULL;
   }

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
      seeqerr = 9;
      free(keys); free(rkeys);
      return NULL;
   }

   // Allocate DFAs.
   dfa_t * dfa = dfa_new(wlen, mismatches, INITIAL_DFA_SIZE, INITIAL_TRIE_SIZE, maxmemory);
   if (dfa == NULL) {
      free(keys); free(rkeys);
      return NULL;
   }

   dfa_t * rdfa = dfa_new(wlen, mismatches, INITIAL_DFA_SIZE, INITIAL_TRIE_SIZE, maxmemory);
   if (rdfa == NULL) {
      free(keys); free(rkeys); free(dfa);
      return NULL;
   }

   // Create seeq object.
   seeq_t * sq = malloc(sizeof(seeq_t));
   if (sq == NULL) {
      free(keys); free(rkeys); free(dfa); free(rdfa);
      return NULL;
   }

   // Set seeq_t struct.
   sq->tau    = mismatches;
   sq->wlen   = wlen;
   sq->keys   = keys;
   sq->rkeys  = rkeys;
   sq->dfa    = (void *) dfa;
   sq->rdfa   = (void *) rdfa;
   sq->bufsz  = 0;
   sq->string = NULL;

   // Initialize match_t stack.
   sq->hits = 0;
   sq->stacksize = INITIAL_MATCH_STACK_SIZE;
   sq->match  = malloc(sq->stacksize * sizeof(match_t));
   if (sq->match == NULL) {
      free(keys); free(rkeys); free(dfa); free(rdfa); free(sq);
      return NULL;
   }

   return sq;
}

void
seeqFree
(
 seeq_t * sq
)
// SYNOPSIS:                                                              
//   Safely frees a seeq_t structure created with 'seeqNew'. This function must be used
//   instead of 'free()', otherwise the references to the internal structures will be lost.
//                                                                        
// PARAMETERS:                                                            
//   sq       : pointer to the seeq_t structure.
//
// RETURN:                                                                
//   void.
//
// SIDE EFFECTS:
//   The memory pointed by sq will be freed.
{
   // Free string if allocated.
   if (sq->string != NULL) free(sq->string);
   // Free keys and match stack.
   free(sq->match);
   free(sq->keys);
   free(sq->rkeys);
   // Free DFAs.
   dfa_free(sq->dfa);
   dfa_free(sq->rdfa);
   free(sq);
}


long
seeqStringMatch
(
 const char * data,
 seeq_t     * sq,
 int          options
)
// SYNOPSIS:                                                              
//   Finds a pattern in the string 'data'. The matching pattern and distance are the ones
//   specified in the call to seeqNew().
//                                                                        
// PARAMETERS:                                                            
//   data    : string to match.
//   sq      : pointer to a seeq_t structure. (see 'seeqNew')
//   options : matching options. Set to 0 for default (SQ_FIRST|SQ_FAIL|SQ_LINES).
//             Bitwise-OR the following macros to set different options.
//             Macros from the same group are mutually exclusive. If two options from
//             the same group are set, undefined behavior occurs.
//
//             MATCH OPTIONS:
//             * SQ_FIRST: searches the line until the first match is found. [DEFAULT]
//             * SQ_BEST: searches the whole line to find the best match, i.e. the one with
//               minimum matching distance. In case of many best matches, the first is stored
//               in 'sq'.
//             * SQ_ALL: finds and stores in 'sq' all the matching positions of the line.
//
//             NON-DNA TEXT OPTIONS:
//             * SQ_FAIL: stops searching the current line if an illegal character is
//               found (allowed characters are 'a','A','c','C','g','G','t','T','u','U','n','N',
//               '\0','\n'). [DEFAULT]
//             * SQ_IGNORE: illegal characters will be ignored.
//             * SQ_CONVERT: illegal characters will be substituted by mismatches ('N').
//
//             INPUT OPTIONS:
//             * SQ_LINES: Search until '\n' or '\0' is found. [DEFAULT]
//             * SQ_STREAM: Search until '\0' is found, newline characters will be ignored.
//             
// RETURN:                                                                
//   Returns the number of matches stored in 'sq', 0 if none was found or -1 in case of error and
//   seeqerr is set appropriately. 
//
// SIDE EFFECTS:
//   The match stack and the cached string of 'sq' are modified.
{
   // Set error to 0.
   seeqerr = 0;

   // Count replaces all other options.
   int match_opt = options & MASK_MATCH;
   int opt_best  = match_opt == SQ_BEST;
   int all_match = match_opt == SQ_ALL || opt_best;

   int nondna_opt = options & MASK_NONDNA;
   int opt_ignore = nondna_opt == SQ_IGNORE;
   const int * translate = translate_ignore;
   if (nondna_opt == SQ_CONVERT) translate = translate_convert;

   int stream_opt = options & MASK_INPUT;

   // Allocate match stacks.
   //   mstack_t ** mstack = malloc((size_t)(sq->tau+1)*sizeof(mstack_t*));
   //   if (mstack == NULL) return -1;
   //   for (int i = 0; i <= sq->tau; i++)
   //      if((mstack[i] = stackNew(INITIAL_MATCH_STACK_SIZE)) == NULL) return -1;

   // Set structure to non-matched.
   sq->hits = 0;

   // Reset search variables
   int best_d = sq->tau + 1;
   // Search variables
   int streak_dist = sq->tau+1;
   int match = 0;
   uint32_t current_node = DFA_ROOT_STATE;
   size_t slen = strlen(data);
   size_t state_size = ((dfa_t *) sq->dfa)->state_size;
   int end = 0;
   
   // DFA state.
   for (size_t i = 0; i <= slen; i++) {
      // Update DFA.
      int cin = (int)translate[(int)data[i]];
      int current_dist = sq->tau+1;
      size_t min_to_match = 0;
      if (cin < NBASES) {
         vertex_t * vertex = (vertex_t *) (((dfa_t *)sq->dfa)->states + current_node * state_size);
         uint32_t next = vertex->next[cin];
         if (next == DFA_COMPUTE)
            if (dfa_step(current_node, cin, sq->wlen, sq->tau, (dfa_t **) &(sq->dfa), sq->keys, &next)) return -1;
         current_node = next;
         vertex = (vertex_t *) (((dfa_t *)sq->dfa)->states + current_node * state_size);
         current_dist = get_match(vertex->match);
         min_to_match = (size_t) get_mintomatch(vertex->match);
      }
      else if (cin == 6 && stream_opt) continue;
      else if (cin == 7 && opt_ignore) continue;
      else if (cin >= 5) {
         current_dist = sq->tau + 1;
         end = 1;
      }

      if (slen - i - 1 < (size_t) min_to_match) {
         current_dist = sq->tau + 1;
         end = 1;
      }

      // Accept matches again.
      if (streak_dist >= current_dist) match = 0;

      // Match accept condition:
      // If there is overlap with the previous, accept only if new_dist < last_dist.
      // 
      // Note:
      // set streak_dist <= current_dist to find all non-overlapping matches.
      // (this may add extra mismatches to a perfect match though)
      if (streak_dist <= sq->tau && streak_dist < current_dist && !match && (!opt_best || streak_dist < best_d)) {
         match = 1;
         size_t j = 0;
         uint32_t rnode = DFA_ROOT_STATE;
         int d = sq->tau + 1;
         // Find match start with RDFA.
         do {
            int c = (int)translate[(int)data[i-++j]];
            if (c < NBASES) {
               vertex_t * vertex = (vertex_t *) (((dfa_t *)sq->rdfa)->states + rnode * state_size);
               uint32_t next = vertex->next[c];
               if (next == DFA_COMPUTE)
                  if (dfa_step(rnode, c, sq->wlen, sq->tau, (dfa_t **) &(sq->rdfa), sq->rkeys, &next)) return -1;
               rnode = next;
               vertex = (vertex_t *) (((dfa_t *)sq->rdfa)->states + rnode * state_size);
               d = get_match(vertex->match);
            } else continue;
         } while (d > streak_dist && j < i);
         size_t match_start = i-j;
         size_t match_end   = i;
         int match_dist  = streak_dist;
         match_t hit = (match_t) {match_start, match_end, (size_t) match_dist};
         // Save non-overlapping matches.
         if (opt_best) {
            // Save match.
            sq->hits = 1;
            sq->match[0] = hit;
            best_d = match_dist;
         } else {
            if (seeqAddMatch(sq,hit)) return -1;
         }
         // Break if done.
         if (!all_match) end = 1;
      }


      // Check end value.
      if (end) break;

      // Track distance.
      streak_dist = current_dist;
   }
   // Merge matches.
   //if(recursive_merge(0, slen, 0, sq, mstack)) return -1;
   // Free mstack.
   //   for (int i = 0; i <= sq->tau; i++) free(mstack[i]);
   //   free(mstack);
   // Swap matches (to compensate for recursive_merge).
   for (int i = 0; i < sq->hits/2; i++) {
      match_t tmp = sq->match[i];
      sq->match[i] = sq->match[sq->hits-i-1];
      sq->match[sq->hits-i-1] = tmp;
   }
   // Return.
   return (long)sq->hits;
}


int
recursive_merge
(
 size_t      start,
 size_t      end,
 int         tau,
 seeq_t    * sq,
 mstack_t ** stackp
)
{
   if (tau > sq->tau) return 0;
   mstack_t   * stack = stackp[tau];
   size_t next_start;
   size_t next_end   = end;
   
   if (stack->pos > 0) {
      // Remove upper overlaps.
      match_t match = stack->match[stack->pos-1];
      while (match.end > end && stack->pos > 0) match = stack->match[--stack->pos-1];

      while (stack->pos > 0) {
         match = stack->match[stack->pos-1];
         if (start > match.start) break;
         // Move match limits.
         next_start =  match.end;
         if (recursive_merge(next_start, next_end, tau+1, sq, stackp)) return -1;
         next_end = match.start;
         // We can safely insert without overlap.
         stack->pos--;
         if (seeqAddMatch(sq, match)) return -1;
      }
   }
   next_start = start;
   if (recursive_merge(next_start, next_end, tau+1, sq, stackp)) return -1;
   return 0;
}

mstack_t *
stackNew
(
 size_t size
 )
{
   if (size < 1) size = 1;
   mstack_t * stack = malloc(sizeof(mstack_t) + size * sizeof(match_t));
   if (stack == NULL) return NULL;
   stack->size = size;
   stack->pos = 0;
   return stack;
}

int
stackAddMatch
(
 mstack_t ** stackp,
 match_t    match
)
{
   mstack_t * stack = *stackp;
   // Realloc stack
   if (stack->pos >= stack->size) {
      size_t newsize = stack->size * 2;
      *stackp = stack = realloc(stack, sizeof(mstack_t) + newsize * sizeof(match_t));
      if (stack == NULL) return -1;
      stack->size = newsize;
   }

   stack->match[stack->pos++] = match;
   return 0;
}


int
seeqAddMatch
(
 seeq_t  * sq,
 match_t   match
)
{
   if (sq->hits >= sq->stacksize) {
      size_t newsize = sq->stacksize * 2;
      sq->match = realloc(sq->match, newsize * sizeof(match_t));
      if (sq->match == NULL) return -1;
      sq->stacksize = newsize;
   }

   sq->match[sq->hits++] = match;
   return 0;
}


match_t *
seeqMatchIter
(
 seeq_t * sq
)
// SYNOPSIS:                                                              
//   Iteratively returns the matches found in the last search.
//                                                                        
// PARAMETERS:                                                            
//   sq: a seeq_t struct created with 'seeqNew()'.
//
// RETURN:                                                                
//   A pointer to the match_t structure or NULL when sq is empty.
//
// SIDE EFFECTS:
//   None.
{
   if (sq->hits == 0) return NULL;
   return sq->match + --sq->hits;
}

char *
seeqGetString
(
 seeq_t * sq
)
// SYNOPSIS:                                                              
//   Returns a pointer to the last matched line.
//                                                                        
// PARAMETERS:                                                            
//   sq: a seeq_t struct created with 'seeqNew()'.
//
// RETURN:                                                                
//   Returns a pointer to the last matched line. The contents of the pointer will be
//   modified if the passed structure is used to match again.
//
// SIDE EFFECTS:
//   None.
{
   return sq->string;
}

const char *
seeqPrintError
(
 void
)
// SYNOPSIS:                                                              
//   Returns a description of the error produced in the last call to a libseeq function.
//                                                                        
// PARAMETERS:                                                            
//   None.
//
// RETURN:                                                                
//   A string with the error description.
//
// SIDE EFFECTS:
//   None.
{
   if (seeqerr == 0) return strerror(errno);
   else return seeq_strerror[seeqerr];
}

 

int
parse
(
 const char * expr,
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
   // Set error to 0.
   seeqerr = 0;
   // Initialize keys to 0.
   for (size_t i = 0; i < strlen(expr); i++) keys[i] = 0;

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
         if (add) {
            seeqerr = 2;
            return -1;
         }
         add = 1;
      }
      else if (c == ']') {
         if (!add) {
            seeqerr = 3;
            return -1;
         }
         if (lc == '[') l--;
         add = 0;
      }
      else {
         seeqerr = 4;
         return -1;
      }

      if (!add) l++;
      i++;
      lc = c;
   }
   
   if (add == 1) {
      seeqerr = 5;
      return -1;
   }
   else return l;
}


dfa_t *
dfa_new
(
 int wlen,
 int tau,
 size_t vertices,
 size_t trienodes,
 size_t maxmemory
)
// SYNOPSIS:                                                              
//   Creates and initializes a new dfa graph with a cache and a root vertices and the
//   specified number of preallocated (empty) vertices. Initializes a trie of height wlen
//   to store the alignment rows. The DFA network is initialized with the first
//   NW-alignment row: [0 1 2 ... tau tau+1 tau+1 ... tau+1]. States 0 and 1 are the
//   cache and root states, respectively.
//                                                                        
// PARAMETERS:                                                            
//   wlen: length of the pattern as regurned by 'parse'.
//   tau: mismatch threshold.
//   vertices: the number of preallocated vertices.
//   trienodes: initial size of the trie.
//   maxmemory: DFA memory limit, in bytes.
//                                                                        
// RETURN:                                                                
//   On success, the function returns a pointer to the new dfa_t structure.
//   A NULL pointer is returned in case of error.
//
// SIDE EFFECTS:
//   The returned dfa_t struct is allocated using malloc and must be manually freed.
{
   // Set error to 0.
   seeqerr = 0;

   if (vertices < 2) vertices = 2;
   if (wlen < 1 || tau < 0) return NULL;

   size_t align_size = (size_t)wlen/5 + (wlen%5 > 0);
   size_t state_size = align_size + sizeof(vertex_t);

   // Allocate DFA.
   dfa_t * dfa = malloc(sizeof(dfa_t) + vertices * state_size);
   if (dfa == NULL) {
      return NULL;
   }

   // Fill struct.
   dfa->size = vertices;
   dfa->pos  = 2;
   dfa->maxmemory = maxmemory;
   dfa->state_size = state_size;
   dfa->align_cache = calloc((size_t)(wlen + 1),sizeof(int));
   dfa->trie = trie_new(trienodes, (size_t)wlen);

   if (dfa->trie == NULL) {
      free(dfa);
      return NULL;
   }

   // Initialize state 0 (cache) and state 1 (root).
   vertex_t * s0 = (vertex_t *) (dfa->states);
   vertex_t * s1 = (vertex_t *) (dfa->states + state_size);
   s0->match = s1->match = set_mintomatch(wlen-tau) | ((uint32_t) tau+1);
   for (int i = 0; i < NBASES; i++) {
      s0->next[i] = DFA_COMPUTE;
      s1->next[i] = DFA_COMPUTE;
   }

   // Allocate memory for path and its encoded version.
   uint8_t * path = malloc((size_t)wlen);
   if (path == NULL || dfa->align_cache == NULL) {
      free(path); free(dfa->trie); free(dfa);
      return NULL;
   }

   // Compute initial alignment.
   for (int i = 0; i <= tau; i++) path[i] = 2;
   for (int i = tau + 1; i < wlen; i++) path[i] = 1;

   // Compute differential code of the path.
   path_encode(path,s1->code,(size_t)wlen);

   // Insert initial state into trie.
   if (trie_insert(dfa, path, 1)) {
      free(path); free(dfa->trie); free(dfa);
      return NULL;
   }
   free(path);

   return dfa;
}


int
dfa_step
(
 uint32_t   dfa_state,
 int        base,
 int        plen,
 int        tau,
 dfa_t   ** dfap,
 char     * exp,
 uint32_t * dfa_next
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
//   base      : next base to resolve (0 for 'A', 1 for 'C', 2 for 'G', 3 for 'T'/'U' and 4 for 'N').
//   plen      : length of the pattern, as returned by 'parse()'.
//   tau       : Levenshtein distance threshold.
//   dfap      : pointer to a memory space containing the address of the DFA.
//   exp       : expression keys, as returned by parse.
//   dfa_next  : a pointer to an uint32_t where the computed DFA transition will be placed,
//
// RETURN:                                                                
//   dfa_step returns 0 on success, or -1 if an error occurred.
//
// SIDE EFFECTS:
//   The DFA structure may be reallocated. The contents of *dfa_next will be
//   overriden.
{
   // Set error to 0.
   seeqerr = 0;

   uint32_t state = dfa_state;
   dfa_t    * dfa = *dfap;
   int      value = 1 << base;

   // Vertex reference.
   vertex_t * vertex = (vertex_t *) (dfa->states + state * dfa->state_size);

   // Return next vertex if already computed.
   if (vertex->next[base] != DFA_COMPUTE) {
      *dfa_next = vertex->next[base];
      return 0;
   }
   
   // Get the current alignment from the DFA state.
   int      * align  = dfa->align_cache;
   uint8_t  * path   = malloc((size_t)plen * sizeof(char));
   if (path == NULL) return -1;

   // Restore alignment if not running in cached mode.
   // TODO:
   // Avoid the cache miss of the alignment. The alignment could be stored
   // in the same dfa vertex structure, but then dynamic size would be needed.
   // This could be implemented converting the stack of vertices into a char*
   // and computing the memory offset of each vertex depending on the pattern
   // length and the size of the compressed alignment
   // (that is: dfa_t is ceil(pattern length/5.0) bytes bigger)
   if (state != 0) {
      path_decode(vertex->code, path, (size_t)plen);
      path_to_align(path, align, (size_t)plen);
   }
 
   // Initialize first column.
   int nextold, prev, old = align[0];
   align[0] = prev = 0;
   int last_active = 1;

   // Update row.
   // TODO:
   // This could be done much faster using a precomputed table of state transitions.
   // Depending on what is the current i-th differential transition, the updated i-th
   // differential transition, the current i+1-th differential transition and whether,
   // the i+1-th position is in match or mismatch.
   // This is a graph with 5 states (current transition and updated transition), each
   // with 6 state transition that emits the i+1-th updated transition.
   for (int i = 1; i < plen+1; i++) {
      nextold   = align[i];
      align[i]  = min(tau + 1, min(old + ((value & exp[i-1]) == 0), min(prev, align[i]) + 1));
      if (align[i] <= tau) last_active = i;
      path[i-1] = (uint8_t)(align[i] - prev + 1);
      prev      = align[i];
      old       = nextold;
   }

   // Save match value.
   uint32_t match = ((uint32_t)prev | set_mintomatch(plen - last_active));
   
   // Check if this state already exists.
   uint32_t dfalink;

   // UPDATE:
   // The entire DFA should be passed to find the remaining path stored
   // in the DFA node (using path_compare).
   int exists = trie_search(dfa, path, &dfalink);

   if (exists == 1) {
      // If exists, just link with the existing state.
      *dfa_next = dfalink;
      // Update edge in dfa.
      if (state != 0) vertex->next[base] = dfalink;   
   } else if (state == 0) {
      *dfa_next = 0;
      vertex_t * s0 = (vertex_t *) dfa->states;
      s0->match = match;
   } else if (exists == 0) {
      int retval = dfa_newstate(dfap, path, base, state);
      dfa = *dfap;
      vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
      if (retval == 0) {
         // Update edge in dfa.
         *dfa_next = vertex->next[base];
         vertex_t * new_vertex = (vertex_t *) (dfa->states + vertex->next[base] * dfa->state_size);
         new_vertex->match = match;
      }
      else  if (retval == 1) {
         // Set to cache mode if max memory is reached.
         *dfa_next = 0;
         vertex_t * s0 = (vertex_t *) dfa->states;
         s0->match = match;
      }
      else {
         free(path);
         return -1;
      }
   }
   else if (exists == -1) {
      free(path);
      return -1;
   }

   free(path);

   return 0;
}


uint32_t
dfa_newvertex
(
 dfa_t   ** dfap
)
// SYNOPSIS:                                                              
//   Adds a new vertex to the dfa graph. The new vertex is not linked to any other
//   vertex in any way, so the connection must be done manually.
//                                                                        
// PARAMETERS:                                                            
//   dfap : pointer to a memory space containing the address of the DFA.
//                                                                        
// RETURN:                                                                
//   On success, the function returns the id of the new vertex. In case of error
//   the function returns U32T_ERROR.
//
// SIDE EFFECTS:
//   If the dfa has reached its maximum size, the dfa will be reallocated
//   doubling its current size. The address of the dfa may have changed after
//   calling dfa_newvertex.
{
   // Set error to 0.
   seeqerr = 0;

   dfa_t * dfa = *dfap;

   // Create new vertex in DFA graph.
   if (dfa->pos >= dfa->size) {
      size_t newsize = dfa->size * 2;
      if (newsize > ABS_MAX_POS) newsize = ABS_MAX_POS;
      *dfap = dfa = realloc(dfa, sizeof(dfa_t) + newsize * dfa->state_size);
      if (dfa == NULL) return U32T_ERROR;
      dfa->size = newsize;
   }

   // Initialize DFA vertex.
   vertex_t * new_vertex = (vertex_t *)(dfa->states + dfa->pos * dfa->state_size);
   new_vertex->match = DFA_COMPUTE;
   for (int j = 0; j < NBASES; j++) new_vertex->next[j] = DFA_COMPUTE;

   // Increase counter.
   return (uint32_t)(dfa->pos++);
}

int
dfa_newstate
(
 dfa_t   ** dfap,
 uint8_t  * path,
 int        edge,
 size_t     dfa_state
)
// SYNOPSIS:                                                              
//   Creates a new vertex to allocate a new DFA state, which represents an 
//   unseen NW alignment row. This function inserts the new NW alignment row in
//   the trie and connects the new vertex with its origin (the current DFA state).
//                                                                        
// PARAMETERS:                                                            
//   dfap      : pointer to a memory space containing the address of the DFA.
//   path      : path of the trie that represents the new NW alignment.
//   dfa_state : current DFA state.
//   edge      : the edge slot to use of the current DFA state.
//                                                                        
// RETURN:                                                                
//   On success, the function returns 0, 1 when the memory limit
//   has been reached or -1 if an error occurred.
//
// SIDE EFFECTS:
//   The dfa may be reallocated, so the content of *dfap may be different
//   at the end of the funcion.
{
   // Set error to 0.
   seeqerr = 0;

   // Check memory usage.
   size_t memory = (*dfap)->pos * (*dfap)->state_size; // DFA memory.
   memory += (*dfap)->trie->pos * sizeof(node_t); // Trie memory.
   if ((*dfap)->maxmemory > 0 && memory > (*dfap)->maxmemory) {
      (*dfap)->trie = realloc((*dfap)->trie, sizeof(trie_t) + (*dfap)->trie->pos * sizeof(node_t));
      if ((*dfap)->trie == NULL) return -1;
      (*dfap)->trie->size = (*dfap)->trie->pos;
      *dfap = realloc(*dfap, sizeof(dfa_t) + (*dfap)->pos * (*dfap)->state_size);
      if (*dfap == NULL) return -1;
      (*dfap)->size = (*dfap)->pos;
      return 1;
   }
   // Absolute memory limit.
   if ((*dfap)->pos == ABS_MAX_POS) return 1;

   // Create new vertex in dfa graph.
   uint32_t vertexid = dfa_newvertex(dfap);
   if (vertexid == U32T_ERROR) return -1;
   vertex_t * old_vertex = (vertex_t *) ((*dfap)->states + dfa_state * (*dfap)->state_size);
   vertex_t * new_vertex = (vertex_t *) ((*dfap)->states + vertexid * (*dfap)->state_size);

   // Encode path.
   path_encode(path, new_vertex->code, (*dfap)->trie->height);

   // Connect dfa vertices.
   old_vertex->next[edge] = (uint32_t) vertexid;

   if (trie_insert(*dfap, path, vertexid)) {
      // Delete DFA state.
      (*dfap)->pos--;
      return -1;
   }

   // Insert new state in the trie.
   return 0;
}

void
dfa_free
(
 dfa_t * dfa
)
{
   if (dfa->align_cache != NULL) free(dfa->align_cache);
   if (dfa->trie != NULL)        free(dfa->trie);
   free(dfa);
}

trie_t *
trie_new
(
 size_t initial_size,
 size_t height
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
   // Set error to 0.
   seeqerr = 0;

   // Allocate at least one node.
   if (initial_size < 1) initial_size = 1;
   if (height < 1) height = 1;

   trie_t * trie = malloc(sizeof(trie_t) + initial_size*sizeof(node_t));
   if (trie == NULL) return NULL;

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
 dfa_t    * dfa,
 uint8_t  * path,
 uint32_t * dfastate
 )
// SYNOPSIS:                                                              
//   Searches the trie following a specified path and returns the values stored
//   at the leaf.
//                                                                        
// PARAMETERS:                                                            
//   dfa      : Pointer to the dfa structure.
//   path     : The path as an array of chars containing values {0,1,2}
//   dfastate : Pointer where the leaf dfastate will be placed (if found).
//                                                                        
// RETURN:                                                                
//   trie_search returns 1 if the path was found, and 0 otherwise. If an
//   error occurred during the search, -1 is returned.
//
// SIDE EFFECTS:
//   None.
{
   // Set error to 0.
   seeqerr = 0;

   trie_t * trie = dfa->trie;

   size_t id = 0;
   size_t i;
   for (i = 0; i < trie->height; i++) {
      if (path[i] >= TRIE_CHILDREN || path[i] < 0) {
         seeqerr = 6;
         return -1;
      }
      // Check if current node is a leaf.
      if (trie->nodes[id].flags & (((uint32_t)1)<<path[i])) {
         // Compare paths.
         vertex_t * vertex = (vertex_t *)(dfa->states + trie->nodes[id].child[(int)path[i]] * dfa->state_size);
         if (path_compare((uint8_t *)path, vertex->code, trie->height) == 0) return 0;
         else break;
      }
      // Update path.
      id = trie->nodes[id].child[(int)path[i]];
      // Check if next node exists.
      if (id == 0) return 0;
   }
   // Save leaf value.
   if (dfastate != NULL) *dfastate = trie->nodes[id].child[(int)path[i]];
   return 1;
}


int
trie_insert
(
 dfa_t   * dfa,
 uint8_t * path,
 uint32_t  dfastate
)
// SYNOPSIS:                                                              
//   Inserts the specified path in the trie and stores the dfa state at the
//   leaf (last node of the path).
//                                                                        
// PARAMETERS:                                                            
//   dfa      : pointer to the dfa structure that contains the trie.
//   path     : The path as an array of chars containing values {0,1,2}
//   dfastate : The associated DFA vertex containing the full alignment. 
//                                                                        
// RETURN:                                                                
//   On success the function returns 0, -1 is returned if an error occurred.
//
// SIDE EFFECTS:
//   If the trie has reached its limit of allocated nodes, it will be reallocated
//   doubling its size. The address of the trie may have changed after calling dfa_insert.
{
   // Set error to 0.
   seeqerr = 0;

   // Check path.
   for (size_t i = 0; i < dfa->trie->height; i++) {
      if (path[i] < 0 || path[i] >= TRIE_CHILDREN) {
         // Bad path, revert trie and return.
         seeqerr = 8;
         return -1;
      }
   }

   size_t id = 0;
   size_t i;
   for (i = 0; i < dfa->trie->height - 1; i++) {
      // Check if the current node is an intermediate leaf.
      if (dfa->trie->nodes[id].flags & (((uint32_t)1)<<path[i])) {
         size_t auxid = id;
         // Save data.
         uint32_t tmpdfa = dfa->trie->nodes[id].child[(int)path[i]];
         // Get the other node's full path.
         uint8_t * tmppath = malloc(dfa->trie->height);
         if (tmppath == NULL) return -1;
         vertex_t * vertex = (vertex_t *) (dfa->states + tmpdfa * dfa->state_size);
         path_decode(vertex->code, tmppath, dfa->trie->height);
         // Unflag leaf.
         dfa->trie->nodes[auxid].flags &= ~(((uint32_t)1)<<path[i]);
         // Move down the node.
         size_t j = i;
         while ((uint8_t) path[j] == tmppath[j] && j < dfa->trie->height) {
            if (dfa->trie->pos == ABS_MAX_POS) {
               // Memory limit reached. Revert movement.
               dfa->trie->nodes[id].flags |= (((uint32_t)1)<<path[i]);
               dfa->trie->nodes[id].child[(int)path[i]] = tmpdfa;
               return 1;
            }
            uint32_t newid = trie_newnode(&(dfa->trie));
            if (newid == U32T_ERROR) return -1;
            dfa->trie->nodes[auxid].child[(int)tmppath[j]] = newid;
            auxid = newid;
            j++;
         }
         // Copy data and flag leaf.
         dfa->trie->nodes[auxid].child[(int)tmppath[j]] = tmpdfa;
         dfa->trie->nodes[auxid].flags |= (((uint32_t)1) << tmppath[j]);
         free(tmppath);
      }

      // Walk the tree.
      if (dfa->trie->nodes[id].child[(int)path[i]] != 0) {
         // Follow path.
         id = dfa->trie->nodes[id].child[(int)path[i]];
      } else break;
   }

   // Write leaf: Store DFA reference vertex and flag leaf.
   dfa->trie->nodes[id].child[(int)path[i]] = dfastate;
   dfa->trie->nodes[id].flags |= (((uint32_t)1)<<path[i]);

   return 0;
}

uint32_t
trie_newnode
(
 trie_t ** triep
)
{
   trie_t * trie = *triep;

   // Check trie size.
   if (trie->pos >= trie->size) {
      size_t newsize = trie->size * 2;
      if (newsize > ABS_MAX_POS) newsize = ABS_MAX_POS;
      *triep = trie = realloc(trie, sizeof(trie_t) + newsize * sizeof(node_t));
      if (trie == NULL) return U32T_ERROR;
      // Initialize new nodes.
      trie->size = newsize;
   }
      
   // Consume one node of the trie.
   size_t newid = trie->pos;
   trie->nodes[newid].child[0] = trie->nodes[newid].child[1] = trie->nodes[newid].child[2] = 0;
   trie->nodes[newid].flags = 0;
   trie->pos++;

   return (uint32_t)newid;
}

void
path_to_align
(
 const uint8_t * path,
 int * align,
 size_t nelements
)
{
   align[0] = 0;
   for (size_t i = 0; i < nelements; i++) align[i+1] = align[i] + path[i] - 1;
}

void
path_encode
(
 const uint8_t * path,
 uint8_t * data,
 size_t nelements
)
// Convert ternary alphabet to binary. (5 ternary symbols per byte)
{
   static const uint8_t power[5] = {81,27,9,3,1};
   // Set memory.
   memset(data, 0, nelements/5 + (nelements%5 > 0));
   for (size_t i = 0; i < nelements; i++) data[i/5] += path[i]%3 * power[i%5];
}

void
path_decode
(
 const uint8_t * data,
 uint8_t * path,
 size_t nelements
)
// Convert binary to ternary alphabet. (One byte yields 5 ternary symbols)
{
   static const uint8_t power[5] = {81,27,9,3,1};
   // Allocate path.
   uint8_t tmp = 0;
   for (size_t i = 0; i < nelements; i++) {
      if (i%5 == 0) tmp = data[i/5];
      path[i] = tmp / power[i%5];
      tmp = tmp % power[i%5];
   }
}


int
path_compare
(
 const uint8_t * path,
 const uint8_t * data,
 size_t nelements
)
// Direct comparion of ternary symbols with its binary representation.
{
   static const uint8_t power[5] = {81,27,9,3,1};
   uint8_t tmp = data[0];
   for (size_t i = 0; i < nelements; i++) {
      if (i % 5 == 0) tmp = data[i/5];
      if (tmp / power[i%5] != path[i]) return 0;
      tmp %= power[i%5];
   }
   return 1;
}
