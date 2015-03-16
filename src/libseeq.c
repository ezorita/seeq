/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
**
** Last modified: March 9, 2015
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

char * seeq_strerror[11] = {   "Check errno",
                               "Illegal matching distance value",
                               "Incorrect pattern (double opening brackets)",
                               "Incorrect pattern (double closing brackets)",
                               "Incorrect pattern (illegal character)",
                               "Incorrect pattern (missing closing bracket)",
                               "Illegal path value passed to 'trie_search'",
                               "Illegal nodeid passed to 'trie_getrow' (specified node is not a leaf).\n",
                               "Illegal path value passed to 'trie_insert'",
                               "Pattern length must be larger than matching distance",
                               "Passed seeq_t struct does not contain a valid file pointer"};

seeq_t *
seeqNew
(
 const char * pattern,
 int          mismatches
)
// SYNOPSIS:                                                              
//   Creates a new seeq_t structure for the defined pattern and matching distance.
//   The pattern is compiled and a DFA structure is created. The DFA network grows
//   each time this structure is used for matching. Use 'seeqFree' to free the
//   structure and the underlying DFA.
//                                                                        
// PARAMETERS:                                                            
//   pattern    : matching pattern (accepted characters 'A','C','G','T','U','N','[',']').
//   mismatches : number of mismatches allowed (Levenshtein distance).
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
   dfa_t * dfa = dfa_new(wlen, mismatches, INITIAL_DFA_SIZE, INITIAL_TRIE_SIZE);
   if (dfa == NULL) {
      free(keys); free(rkeys);
      return NULL;
   }

   dfa_t * rdfa = dfa_new(wlen, mismatches, INITIAL_DFA_SIZE, INITIAL_TRIE_SIZE);
   if (rdfa == NULL) {
      free(keys); free(rkeys); free(dfa);
      return NULL;
   }

   seeq_t * sq = malloc(sizeof(seeq_t));
   if (sq == NULL) {
      free(keys); free(rkeys); free(dfa); free(rdfa);
      return NULL;
   }

   // Set seeq_t struct.
   sq->tau   = mismatches;
   sq->wlen  = wlen;
   sq->keys  = keys;
   sq->rkeys = rkeys;
   sq->dfa   = (void *) dfa;
   sq->rdfa  = (void *) rdfa;
   // Initialize match_t struct.
   sq->match.start  = 0;
   sq->match.end    = 0;
   sq->match.dist   = mismatches + 1;
   sq->match.line   = 0;
   sq->match.string = NULL;

   return sq;
}

void
seeqFree
(
 seeq_t * sq
)
// SYNOPSIS:                                                              
//   Frees a seeq_t structure created with 'seeqNew'. It is important to use this
//   function instead of using free() directly with the seeq_t pointer, otherwise
//   the references to the DFA structures and match cache data will be lost.
//                                                                        
// PARAMETERS:                                                            
//   sq       : pointer to the seeq_t structure.
//
// RETURN:                                                                
//
// SIDE EFFECTS:
//   The memory pointed by sq will be freed.
{
   // Free string if allocated.
   if (sq->match.string != NULL) free(sq->match.string);
   free(sq->keys);
   free(sq->rkeys);
   free(sq->dfa);
   free(sq->rdfa);
   free(sq);
}

seeqfile_t *
seeqOpen
(
 const char * file
)
// SYNOPSIS:                                                              
//   Creates a seeqfile_t structure to match file lines directly against a pattern.
//   The generated structure must be passed to 'seeqFileMatch' jointly with a seeq_t
//   structure compiled with the matching pattern.
//   If a file name is specified, the structure will open the file and update the
//   file pointer automatically after each pattern search. If file is set to NULL,
//   'seeqFileMatch' will read the lines from stdin. Note that many different files
//    can be matched simultaneously with the same pattern (seeq_t structure) or vice
//    versa.
//                                                                        
// PARAMETERS:                                                            
//   file       : name of the file to match. Set to NULL for stdin or string matching.
//
// RETURN:                                                                
//   Returns a pointer to a seeqfile_t structure or NULL in case of error, and seeqerr
//   is set appropriately.
//
// SIDE EFFECTS:
//   The returned seeqfile_t structure must be freed using 'seeqClose'.
{
   // Set error to 0.
   seeqerr = 0;

   seeqfile_t * sqfile = malloc(sizeof(seeqfile_t));
   if (sqfile == NULL) return NULL;

   // Open file or set to stdin
   FILE * fdi = stdin;
   if (file != NULL) fdi = fopen(file, "r");
   if (fdi  == NULL) return NULL;

   sqfile->line = 0;
   sqfile->fdi = fdi;

   return sqfile;
}

int
seeqClose
(
 seeqfile_t * sqfile
)
// SYNOPSIS:                                                              
//   Closes the file (if any) and frees a seeqfile_t structure generated by 'seeqOpen'.
//                                                                        
// PARAMETERS:                                                            
//   sqfile : a pointer to the seeqfile_t structure.
//
// RETURN:                                                                
//   Returns zero on success or -1 in case of error.
//
// SIDE EFFECTS:
//   The memory pointed by sqfile will be freed.
{
   // Set error to 0.
   seeqerr = 0;
   // Close file.
   if (sqfile->fdi != stdin && sqfile->fdi != NULL) if (fclose(sqfile->fdi) != 0) return -1;
   // Free structure.
   free(sqfile);
   return 0;
}

long
seeqFileMatch
(
 seeqfile_t* sqfile,
 seeq_t    * sq,
 int         options
)
// SYNOPSIS:                                                              
//   Find the next match in the file pointed by 'sqfile', starting from the position where
//   the previous call to 'seeqFileMatch' returned. The matching pattern and distance will be
//   the ones specified in the call of 'seeqNew()' used to create 'sq'. The matching behavior
//   can be specified in 'options' using the defined SQ_* macros.
//                                                                        
// PARAMETERS:                                                            
//   sqfile  : pointer to a seeqfile_t structure obtained with 'seeqOpen'.
//   sq      : pointer to a seeq_t structure obtained with 'seeqNew'.
//   options : matching options, the following macros can be bitwise-OR'd:
//             * SQ_COUNTLINES: Counts the number of matching lines, starting from the
//               current position until the end of the file.
//             * SQ_COUNTMATCH: Counts all the matches found in the file that have a Leve-
//               nshtein distance less or equal than the specified in 'seeqOpen()'. Note
//               that there may be more than one match per line.
//
//             * SQ_MATCH: only returns lines that match the pattern.
//             * SQ_NOMATCH: only returns lines that DO NOT match the pattern.
//             * SQ_ANY: returns both matched and non-matched lines (SQ_MATCH | SQ_NOMATCH).
//
//             * SQ_FIRST: searches the line until the first match.
//             * SQ_BEST: searches the whole line to find the best match, i.e. the one with
//               minimum matching distance. In case of many best matches, the first is returned.
//             
//             If SQ_COUNTLINES or SQ_COUNTMATCH are specified, the other options are ignored.
//             Also, the match/line count is passed as the return value.
//             SQ_MATCH and SQ_ANY can be OR'd with SQ_FIRST/SQ_BEST. The non-matching lines
//             are returned with a matching distance of -1 (this can be checked either using
//             'seeqGetDistance()' or directly reading sqfile->match.dist). The matching line
//             and position can also be read after a call to 'seeqMatch' using the 'seeqGet*'
//             functions.
//             If options is set to 0, the default options are (SQ_ANY|SQ_FIRST).
//
// RETURN:                                                                
//   Returns the number of matches found. If zero is returned, the end of the file has been
//   reached without finding any match. In case of error, -1 is returned and seeqerr is set
//   appropriately.
//
// SIDE EFFECTS:
//   The match details of the last match stored in 'sq' overriden and the file pointer offset
//   in 'seeqfile' is updated to the new matching position.
{
   // Set error to 0.
   seeqerr = 0;

   if (sqfile->fdi == NULL) {
      seeqerr = 10;
      return -1;
   }

   // Set structure to non-matched.
   sq->match.start = sq->match.end = sq->match.line = -1;
   sq->match.dist  = sq->tau + 1;

   // Aux vars.
   long startline = sqfile->line;
   size_t bufsz = INITIAL_LINE_SIZE;
   ssize_t readsz;
   long count = 0;

   while ((readsz = getline(&(sq->match.string), &bufsz, sqfile->fdi)) > 0) {
      sqfile->line++;
      // Remove newline.
      char * data = sq->match.string;
      if (data[readsz-1] == '\n') data[readsz---1] = 0;

      // Call String Match
      count += seeqStringMatch(data, sq, options);

      // Break when match is found.
      if (count > 0 && !(options & SQ_COUNTMATCH) && !(options & SQ_COUNTLINES)) {
         sq->match.line = sqfile->line;
         break;
      }
   }

   // If nothing was read, return 0.
   if (sqfile->line == startline) return 0;
   else return count;
}

long
seeqStringMatch
(
 const char* data,
 seeq_t    * sq,
 int         options
)
// SYNOPSIS:                                                              
//   Match the pattern of the seeq_t structure in the string 'data'. The matching behavior
//   can be specified in 'options' using the defined SQ_* macros.
//                                                                        
// PARAMETERS:                                                            
//   data    : string to match.
//   sq      : pointer to a seeq_t structure obtained with 'seeqOpen'.
//   options : matching options, the following macros can be bitwise-OR'd:
//             * SQ_COUNTLINES: Returns 1 if the passed string contains a match.
//             * SQ_COUNTMATCH: Counts all the matches found in the string that have a Leve-
//               nshtein distance less or equal than the specified in 'seeqOpen()'.
//
//             * SQ_MATCH: only returns lines that match the pattern.
//             * SQ_NOMATCH: only returns lines that DO NOT match the pattern.
//             * SQ_ANY: returns both matched and non-matched lines (SQ_MATCH | SQ_NOMATCH).
//
//             * SQ_FIRST: searches the line until the first match.
//             * SQ_BEST: searches the whole line to find the best match, i.e. the one with
//               minimum matching distance. In case of many best matches, the first is returned.
//             
//             If SQ_COUNTLINES or SQ_COUNTMATCH are specified, the other options are ignored.
//             Also, the match/line count is passed as the return value.
//             SQ_MATCH and SQ_ANY can be OR'd with SQ_FIRST/SQ_BEST. The non-matching lines
//             are returned with a matching distance of -1 (this can be checked either using
//             'seeqGetDistance()' or directly reading sqfile->match.dist). The matching line
//             and position can also be read after a call to 'seeqMatch' using the 'seeqGet*'
//             functions.
//             If options is set to 0, the default options are (SQ_ANY|SQ_FIRST).
//
// RETURN:                                                                
//   Returns the number of matches found with the specified options, 0 if none was found or
//   -1 in case of error and seeqerr is set appropriately. 
//
//   Examples:
//   Returns the total number of matches found in the string is SQ_COUNTMATCH is specified.
//   If SQ_MATCH is set and no match is found in the string, the function returns 0.
//   If SQ_NOMATCH is set and the string contains a match, the function returns 0.
//   If SQ_ANY or SQ_MATCH|SQ_NOMATCH is set, the function should always return 1.
//
// SIDE EFFECTS:
//   The match details of the last match stored in 'sq' are overriden.
{
   // Set error to 0.
   seeqerr = 0;

   // Count replaces all other options.
   int countall   = options & SQ_COUNTMATCH;
   int best_match = options & SQ_BEST;
   if (options & (SQ_COUNTLINES|SQ_COUNTMATCH)) options = 0;
   else if ((options & 0x03) == 0) options = SQ_MATCH | SQ_NOMATCH;

   // Set structure to non-matched.
   sq->match.start = sq->match.end = sq->match.line = -1;
   sq->match.dist  = sq->tau + 1;

   // Reset search variables
   int streak_dist = sq->tau+1;
   int current_node = 0;
   long count = 0;
   int match = 0;
   int slen = strlen(data);
   
   // DFA state.
   for (unsigned long i = 0; i <= slen; i++) {
      // Update DFA.
      int cin = (int)translate[(int)data[i]];
      edge_t next;
      if(cin < NBASES) {
         next = ((dfa_t *) sq->dfa)->states[current_node].next[cin];
         if (next.match == DFA_COMPUTE)
            if (dfa_step(current_node, cin, sq->wlen, sq->tau, (dfa_t **) &(sq->dfa), sq->keys, &next)) return -1;
         current_node = next.state;
      }
      else if (cin == 6) {
         // Now lines containing illegal characters will be ommited.
         break;
         // Consider changing this by:
         //   streak_dist = tau + 1;
         //   current_node = 0;
         //   continue;
         // to report these lines as non-matches.
         // Alternatively, translate could redirect illegal characters to 'N'.
      }
      if (cin == 5 || slen - i - 1 < next.min_to_match) {
         next.match = sq->tau+1;
         if ((options & SQ_NOMATCH) && sq->match.start == -1 && streak_dist >= next.match) {
            sq->match.line  = 0;
            sq->match.start = 0;
            sq->match.end   = slen;
            sq->match.dist  = -1;
            count = 1;
            break;
         }
      }

      // Update streak.
      if (streak_dist >= next.match) {
         match = 0;
      } else if (!match && streak_dist < next.match && streak_dist < sq->match.dist) {
         match = 1;
         if (options & SQ_MATCH) {
            long j = 0;
            int rnode = 0;
            int d = sq->tau + 1;
            // Find match start with RDFA.
            do {
               int c = (int)translate[(int)data[i-++j]];
               edge_t next = ((dfa_t *) sq->rdfa)->states[rnode].next[c];
               if (next.match == DFA_COMPUTE)
                  if (dfa_step(rnode, c, sq->wlen, sq->tau, (dfa_t **) &(sq->rdfa), sq->rkeys, &next)) return -1;
               rnode = next.state;
               d     = next.match;
            } while (d > streak_dist && j < i);

            // Compute match length.
            j = i - j;
            // Save match.
            sq->match.start = j;
            sq->match.end   = i;
            sq->match.line  = 0;
            sq->match.dist  = streak_dist;
         } else if (options & SQ_NOMATCH) break;

         if (count == 0 || countall) count++;
         if (!best_match && !countall) break;
      }

      // Track distance.
      streak_dist   = next.match;
   }

   return count;
}


long
seeqGetLine
(
 seeq_t * sq
)
// SYNOPSIS:                                                              
//   Returns the file line of the last match.
//                                                                        
// PARAMETERS:                                                            
//   sq: a seeq_t struct created with 'seeqOpen()' and matched with 'seeqStringMatch()'
//       or 'seeqMatch()'.
//
// RETURN:                                                                
//   Returns the file line of the last match.
//
// SIDE EFFECTS:
//   None.
{
   return sq->match.line;
}

int
seeqGetDistance
(
 seeq_t * sq
)
// SYNOPSIS:                                                              
//   Returns the Levenshtein distance of the last match.
//                                                                        
// PARAMETERS:                                                            
//   sq: a seeq_t struct created with 'seeqOpen()' and matched with 'seeqStringMatch()'
//       or 'seeqMatch()'.
//
// RETURN:                                                                
//   Returns the Levenshtein distance of the last match.
//
// SIDE EFFECTS:
//   None.
{
   return sq->match.dist;
}

int
seeqGetStart
(
 seeq_t * sq
)
// SYNOPSIS:                                                              
//   Returns the start position of the last match.
//                                                                        
// PARAMETERS:                                                            
//   sq: a seeq_t struct created with 'seeqOpen()' and matched with 'seeqStringMatch()'
//       or 'seeqMatch()'.
//
// RETURN:                                                                
//   Returns the start position of the last match.
//
// SIDE EFFECTS:
//   None.
{
   return sq->match.start;
}

int
seeqGetEnd
(
 seeq_t * sq
)
// SYNOPSIS:                                                              
//   Returns the end position of the last match. The position returned points to the
//   first non-matching character after the match.
//                                                                        
// PARAMETERS:                                                            
//   sq: a seeq_t struct created with 'seeqOpen()' and matched with 'seeqStringMatch()'
//       or 'seeqMatch()'.
//
// RETURN:                                                                
//   Returns the end position of the last match.
//
// SIDE EFFECTS:
//   None.
{
   return sq->match.end;
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
//   sq: a seeq_t struct created with 'seeqOpen()' and matched with 'seeqStringMatch()'
//       or 'seeqMatch()'.
//
// RETURN:                                                                
//   Returns a pointer to the last matched line.
//
// SIDE EFFECTS:
//   A string is allocated to store the matched line and must be freed by the user.
{
   if (sq->match.string == NULL) return NULL;
   return strdup(sq->match.string);
}

char *
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
 int vertices,
 int trienodes
)
// SYNOPSIS:                                                              
//   Creates and initializes a new dfa graph with a root vertex and the specified
//   number of preallocated vertices. Initializes a trie to keep the alignment rows
//   with height wlen and some preallocated nodes. The DFA network is initialized
//   with the first NW-alignment row [0 1 2 ... tau tau+1 tau+1 ... tau+1].
//                                                                        
// PARAMETERS:                                                            
//   wlen: length of the pattern as regurned by 'parse'.
//   tau: mismatch threshold.
//   vertices: the number of preallocated vertices.
//   trienodes: initial size of the trie.
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

   if (vertices < 1) vertices = 1;
   if (wlen < 1 || tau < 0) return NULL;

   dfa_t * dfa = malloc(sizeof(dfa_t) + vertices * sizeof(vertex_t));
   if (dfa == NULL) {
      return NULL;
   }

   // Initialize state 0.
   edge_t new = {.state = 0, .match = DFA_COMPUTE};
   for (int i = 0; i < NBASES; i++) {
      dfa->states[0].next[i] = new;
   }

   trie_t * trie = trie_new(trienodes, wlen);
   if (trie == NULL) {
      free(dfa);
      return NULL;
   }

   // Compute initial NFA state.
   char path[wlen+1];
   for (int i = 0; i <= tau; i++) path[i] = 2;
   for (int i = tau + 1; i < wlen; i++) path[i] = 1;

   // Insert initial state into trie.
   uint nodeid = trie_insert(&trie, path, tau+1, 0);
   if (nodeid == (uint)-1) {
      free(trie);
      free(dfa);
      return NULL;
   }

   // Fill struct.
   dfa->size = vertices;
   dfa->pos  = 1;
   dfa->trie = trie;

   // Link trie leaf with DFA vertex.
   dfa->states[0].node_id = nodeid;

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
 char   *  exp,
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
//   base      : next base to resolve (0 for 'A', 1 for 'C', 2 for 'G', 3 for 'T'/'U' and 4 for 'N').
//   plen      : length of the pattern, as returned by parse.
//   tau       : Levenshtein distance threshold.
//   dfap      : pointer to a memory space containing the address of the DFA.
//   trie      : pointer to a memory space containing the address of the associated trie.
//   exp       : expression keys, as returned by parse.
//   nextedge  : a pointer to an edge_t struct where the computed transition will be placed,
//
// RETURN:                                                                
//   dfa_step returns 0 on success, or -1 if an error occurred.
//
// SIDE EFFECTS:
//   The returned dfa_t struct is allocated using malloc and must be manually freed.
{
   // Set error to 0.
   seeqerr = 0;

   dfa_t  * dfa   = *dfap;

   // Return next vertex if already computed.
   if (dfa->states[dfa_state].next[base].match != DFA_COMPUTE) {
      *nextedge = dfa->states[dfa_state].next[base];
      return 0;
   }

   int  value = 1 << base;

   // Explore the trie backwards to find out the NFA state.
   uint * state = trie_getrow(dfa->trie, dfa->states[dfa_state].node_id);
   char * code  = calloc(plen + 1, sizeof(char));

   if (state == NULL) return -1;

   if (code == NULL) return -1;

   // Initialize first column.
   int nextold, prev, old = state[0];
   state[0] = prev = 0;
   int last_active = 1;

   // Update row.
   for (int i = 1; i < plen+1; i++) {
      nextold   = state[i];
      state[i]  = min(tau + 1, min(old + ((value & exp[i-1]) == 0), min(prev, state[i]) + 1));
      if (state[i] <= tau) last_active = i;
      code[i-1] = state[i] - prev + 1;
      prev      = state[i];
      old       = nextold;
   }

   // Save match value.
   dfa->states[dfa_state].next[base].match = prev;
   
   // Check if this state already exists.
   uint dfalink;
   int exists = trie_search(dfa->trie, code, NULL, &dfalink);

   if (exists == 1) {
      // If exists, just link with the existing state.
      dfa->states[dfa_state].next[base].state = dfalink;
   } else if (exists == 0) {
      if (dfa_newstate(dfap, code, prev, dfa_state, base) == -1) return -1;
      // Set min_to_match:
      dfa->states[dfa_state].next[base].min_to_match = plen - last_active;
      dfa = *dfap;
   } else return -1;

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
   // Set error to 0.
   seeqerr = 0;

   dfa_t * dfa = *dfap;

   // Create new vertex in DFA graph.
   if (dfa->pos >= dfa->size) {
      dfa->size *= 2;
      *dfap = dfa = realloc(dfa, sizeof(dfa_t) + dfa->size * sizeof(vertex_t));
      if (dfa == NULL) return -1;
   }

   // Initialize DFA vertex.
   dfa->states[dfa->pos].node_id = nodeid;
   edge_t new = {.state = 0, .match = DFA_COMPUTE};
   for (int j = 0; j < NBASES; j++) dfa->states[dfa->pos].next[j] = new;

   // Increase counter.
   return dfa->pos++;
}

int
dfa_newstate
(
 dfa_t ** dfap,
 char   * code,
 uint     alignval,
 uint     dfa_state,
 int      edge
)
// SYNOPSIS:                                                              
//   Creates a new vertex to allocate a new DFA state, which represents an 
//   unseen NW alignment row. This function inserts the new NW alignment row in
//   the trie and connects the new vertex with its origin (the current DFA state).
//                                                                        
// PARAMETERS:                                                            
//   dfap      : pointer to a memory space containing the address of the DFA.
//   code      : path of the trie that represents the new NW alignment.
//   alignval  : last column of the NW alignment.
//   dfa_state : current DFA state.
//   edge      : the edge slot to use of the current DFA state.
//                                                                        
// RETURN:                                                                
//   On success, the function returns 0, or -1 if an error occurred.
//
// SIDE EFFECTS:
//   The dfa may be reallocated, so the content of *dfap may be different
//   at the end of the funcion.
{
   // Set error to 0.
   seeqerr = 0;

   // Insert new NFA state in the trie.
   uint nodeid = trie_insert(&((*dfap)->trie), code, alignval, (*dfap)->pos);
   if (nodeid == (uint)-1) return -1;
   // Create new vertex in dfa graph.
   uint vertexid = dfa_newvertex(dfap, nodeid);
   if (vertexid == (uint)-1) return -1;
   // Connect dfa vertices.
   (*dfap)->states[dfa_state].next[edge].state = vertexid;
   return 0;
}

void
dfa_free
(
 dfa_t * dfa
)
{
   if (dfa->trie != NULL) free(dfa->trie);
   free(dfa);
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
   // Set error to 0.
   seeqerr = 0;

   // Allocate at least one node.
   if (initial_size < 1) initial_size = 1;
   if (height < 0) height = 0;

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
   // Set error to 0.
   seeqerr = 0;

   uint id = 0;
   for (int i = 0; i < trie->height; i++) {
      if (path[i] >= TRIE_CHILDREN || path[i] < 0) {
         seeqerr = 6;
         return -1;
      }
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
   // Set error to 0.
   seeqerr = 0;

   node_t * nodes = &(trie->nodes[0]);
   uint   * path  = malloc((trie->height + 1) * sizeof(uint));
   int      i     = trie->height;
   uint     id    = nodeid;

   if (path == NULL) return NULL;

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
      seeqerr = 7;
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
   // Set error to 0.
   seeqerr = 0;

   trie_t * trie  = *triep;
   node_t * nodes = &(trie->nodes[0]);
   uint id = 0;
   uint initial_pos = trie->pos;

   int i;
   for (i = 0; i < trie->height; i++) {
      if (path[i] < 0 || path[i] >= TRIE_CHILDREN) {
         // Bad path, revert trie and return.
         trie->pos = initial_pos;
         seeqerr = 8;
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
         if (trie == NULL) return -1;
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
