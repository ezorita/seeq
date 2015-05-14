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

static const char * seeq_strerror[11] = {"Check errno",
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
   sq->match.line   = 0;
   sq->match.dist   = mismatches + 1;
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
   dfa_free(sq->dfa);
   dfa_free(sq->rdfa);
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
 seeqfile_t * sqfile,
 seeq_t     * sq,
 int          options
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
   sq->match.start = sq->match.end = sq->match.line = (size_t)(-1);
   sq->match.dist  = sq->tau + 1;

   // Aux vars.
   size_t startline = sqfile->line;
   size_t bufsz = INITIAL_LINE_SIZE;
   ssize_t readsz;
   long count = 0;

   while ((readsz = getline(&(sq->match.string), &bufsz, sqfile->fdi)) > 0) {
      sqfile->line++;
      // Remove newline.
      char * data = sq->match.string;
      if (data[readsz-1] == '\n') data[--readsz] = 0;

      // Call String Match
      long rval = seeqStringMatch(data, sq, options);
      if (rval == -1) return -1;
      else count += rval;

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
 const char * data,
 seeq_t     * sq,
 int          options
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
//             * SQ_ILLEGAL_STOP: stops searching the current line if an illegal character is
//               found (allowed characters are 'a','A','c','C','g','G','t','T','u','U','n','N',
//               '\0','\n').
//             * SQ_ILLEGAL_CONT: illegal characters will be substituted by mismatches ('N').
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
   const int * translate;
   if (options & SQ_CONT) translate = translate_n;
   else translate = translate_halt;
   if (options & (SQ_COUNTLINES|SQ_COUNTMATCH)) options = 0;
   else if ((options & 0x03) == 0) options = SQ_MATCH | SQ_NOMATCH;

   // Set structure to non-matched.
   sq->match.start = sq->match.end = sq->match.line = (size_t)(-1);
   sq->match.dist  = sq->tau + 1;

   // Reset search variables
   int streak_dist = sq->tau+1;
   int match = 0;
   size_t current_node = 0;
   size_t slen = strlen(data);
   long count = 0;
   
   // DFA state.
   for (size_t i = 0; i <= slen; i++) {
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
      if (cin == 5 || slen - i - 1 < (size_t) next.min_to_match) {
         next.match = sq->tau+1;
         if ((options & SQ_NOMATCH) && sq->match.start == (size_t)-1 && streak_dist >= next.match) {
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
            size_t j = 0;
            size_t rnode = 0;
            int d = sq->tau + 1;
            // Find match start with RDFA.
            do {
               int c = (int)translate[(int)data[i-++j]];
               edge_t rnext = ((dfa_t *) sq->rdfa)->states[rnode].next[c];
               if (rnext.match == DFA_COMPUTE)
                  if (dfa_step(rnode, c, sq->wlen, sq->tau, (dfa_t **) &(sq->rdfa), sq->rkeys, &rnext)) return -1;
               rnode = rnext.state;
               d     = rnext.match;
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


size_t
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

size_t
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

size_t
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
 size_t trienodes
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

   trie_t * trie = trie_new(trienodes, (size_t)wlen);
   if (trie == NULL) {
      free(dfa);
      return NULL;
   }

   // Fill struct.
   dfa->size = vertices;
   dfa->pos  = 1;
   dfa->trie = trie;

   // Compute initial NFA state.
   char * path = malloc((size_t)wlen+1);
   if (path == NULL) return NULL;
   for (int i = 0; i <= tau; i++) path[i] = 2;
   for (int i = tau + 1; i < wlen; i++) path[i] = 1;

   // Insert initial state into trie.
   size_t nodeid = trie_insert(dfa, path, (size_t)tau+1, 0);
   free(path);

   if (nodeid == (size_t)-1) {
      free(trie);
      free(dfa);
      return NULL;
   }

   // Link trie leaf with DFA vertex.
   dfa->states[0].node_id = nodeid;

   return dfa;
}


int
dfa_step
(
 size_t    dfa_state,
 int       base,
 int       plen,
 int       tau,
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
//   plen      : length of the pattern, as returned by 'parse()'.
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
   int  * state = trie_getrow(dfa->trie, dfa->states[dfa_state].node_id);
   char * code  = calloc((size_t)plen + 1, sizeof(char));

   if (state == NULL || code == NULL) return -1;

   // Initialize first column.
   int nextold, prev, old = state[0];
   state[0] = prev = 0;
   int last_active = 1;

   // Update row.
   for (int i = 1; i < plen+1; i++) {
      nextold   = state[i];
      state[i]  = min(tau + 1, min(old + ((value & exp[i-1]) == 0), min(prev, state[i]) + 1));
      if (state[i] <= tau) last_active = i;
      code[i-1] = (char) (state[i] - prev + 1);
      prev      = state[i];
      old       = nextold;
   }

   // Save match value.
   dfa->states[dfa_state].next[base].match = prev;
   
   // Check if this state already exists.
   size_t dfalink;
   int exists = trie_search(dfa->trie, code, NULL, &dfalink);

   if (exists == 1) {
      // If exists, just link with the existing state.
      dfa->states[dfa_state].next[base].state = dfalink;
   } else if (exists == 0) {
      if (dfa_newstate(dfap, code, prev, base, dfa_state) == -1) {
         free(state);
         free(code);
         return -1;
      }
      // Set min_to_match:
      dfa = *dfap;
      dfa->states[dfa_state].next[base].min_to_match = (unsigned int)(plen - last_active);
   } else {
      free(state);
      free(code);
      return -1;
   }

   free(state);
   free(code);
   
   *nextedge = dfa->states[dfa_state].next[base];
   return 0;
}


size_t
dfa_newvertex
(
 dfa_t ** dfap,
 size_t   nodeid
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
//   the function returns (size_t)-1.
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
      if (dfa == NULL) return (size_t)-1;
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
 int      alignval,
 int      edge,
 size_t   dfa_state
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
   size_t nodeid = trie_insert(*dfap, code, (size_t)alignval, (*dfap)->pos);
   if (nodeid == (size_t)-1) return -1;

   // Create new vertex in dfa graph.
   size_t vertexid = dfa_newvertex(dfap, nodeid);
   if (vertexid == (size_t)-1) return -1;

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
 size_t * value,
 size_t * dfastate
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

   size_t id = 0;
   for (size_t i = 0; i < trie->height; i++) {
      if (path[i] >= TRIE_CHILDREN || path[i] < 0) {
         seeqerr = 6;
         return -1;
      }
      // Check if current node is a leaf.
      if (trie->nodes[id].child[2] & type_msb(trie->nodes[id].child[2])) {
         // Compare paths.
         if (trie_compare((unsigned char *)path + i, (unsigned char *) trie->nodes[id].child[0], trie->height - i) == 0) return 0;
         else break;
      }
      // Update path.
      id = trie->nodes[id].child[(int)path[i]];
      // Check if next node exists.
      if (id == 0) return 0;
   }
   // Save leaf value.
   if (value != NULL) *value = trie->nodes[id].child[2] & ~type_msb(trie->nodes[id].child[2]);
   if (dfastate != NULL) *dfastate = trie->nodes[id].child[1];
   return 1;
}


int *
trie_getrow
(
 trie_t * trie,
 size_t   nodeid
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

   // Alloc path.
   node_t * nodes = &(trie->nodes[0]);
   int    * path  = malloc((trie->height + 1) * sizeof(int));
   if (path == NULL) return NULL;

   if(!(nodes[nodeid].child[2] & type_msb(nodes[nodeid].child[2]))) {
      // Set error flag.

      // Return
      return NULL;
   }

   // Find height of the node.
   size_t height = 0;
   size_t node   = nodeid;
   while (node != 0) {
      height++;
      node = nodes[node].parent;
   }

   path[trie->height] = (int) (nodes[nodeid].child[2] & ~type_msb(nodes[nodeid].child[2]));
   
   // Decode stored path.
   if (height < trie->height) {
      unsigned char * dec_path = trie_decode((unsigned char *)nodes[nodeid].child[0], trie->height - height);
      for (size_t i = trie->height; i > height; i--) {
         path[i-1] = path[i] - (int)dec_path[i-1-height] + 1;
      }
      free(dec_path);
   }

   // Compute row from trie path.
   for (size_t i = height, id = nodeid; i > 0; i--) {
      size_t next_id = nodes[id].parent;
      path[i-1] = (int)(path[i] + (nodes[next_id].child[0] == id) - (nodes[next_id].child[2] == id));
      id = next_id;
   } 

   return path;
}


size_t
trie_insert
(
 dfa_t  * dfa,
 char   * path,
 size_t   value,
 size_t   dfastate
)
// SYNOPSIS:                                                              
//   Inserts the specified path in the trie and stores the end value and
//   the dfa state at the leaf (last node of the path). If the path already
//   exists, its leaf values will be overwritten.
//                                                                        
// PARAMETERS:                                                            
//   trie     : pointer to a memory space containing the address of the trie.
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

   size_t id = 0;
   size_t initial_pos = dfa->trie->pos;

   size_t i;
   for (i = 0; i < dfa->trie->height; i++) {
      if (path[i] < 0 || path[i] >= TRIE_CHILDREN) {
         // Bad path, revert trie and return.
         dfa->trie->pos = initial_pos;
         seeqerr = 8;
         return (size_t)-1;
      }

      // Check if the current node is an intermediate leaf.
      if (dfa->trie->nodes[id].child[2] & type_msb(dfa->trie->nodes[id].child[2])) {
         size_t auxid = id;
         // Save data.
         unsigned char * data   = (unsigned char *) dfa->trie->nodes[id].child[0];
         size_t tmpdfa = dfa->trie->nodes[id].child[1];
         size_t tmpval = dfa->trie->nodes[id].child[2];
         // Reset node.
         dfa->trie->nodes[id].child[0] = dfa->trie->nodes[id].child[1] = dfa->trie->nodes[id].child[2] = 0;
         // Move down the node.
         size_t coded_path_len = dfa->trie->height - i;
         unsigned char * tmppath = trie_decode(data, coded_path_len);
         size_t j = 0;
         do {
            size_t newid = trie_newnode(&(dfa->trie), auxid);
            dfa->trie->nodes[auxid].child[(int)tmppath[j]] = newid;
            auxid = newid;
            j++; coded_path_len--;
         } while ((unsigned char) path[i+j] == tmppath[j] && i+j < dfa->trie->height);
         // Recompute encoded path.
         unsigned char * newdata;
         if (coded_path_len) {
            newdata = trie_encode(tmppath+j, coded_path_len);
         } else newdata = NULL;
         // Free old path.
         free(tmppath);
         free(data);
         // Copy data.
         dfa->trie->nodes[auxid].child[0] = (size_t) newdata;
         dfa->trie->nodes[auxid].child[1] = tmpdfa;
         dfa->trie->nodes[auxid].child[2] = tmpval;
            
         // Update DFA pointer.
         dfa->states[tmpdfa].node_id = auxid;
      }

      // Walk the tree.
      if (dfa->trie->nodes[id].child[(int)path[i]] != 0) {
         // Follow path.
         id = dfa->trie->nodes[id].child[(int)path[i]];
      } else break;
   }

   // Create new leaf. 
   size_t newid = trie_newnode(&(dfa->trie), id);
   dfa->trie->nodes[id].child[(int)path[i++]] = newid;

   // Write leaf information.
   unsigned char * data;
   if (i < dfa->trie->height)
      data = trie_encode((unsigned char *)path + i, dfa->trie->height - i);
   else data = NULL;
   dfa->trie->nodes[newid].child[0] = (size_t) data;
   dfa->trie->nodes[newid].child[1] = dfastate;
   dfa->trie->nodes[newid].child[2] = value | type_msb(dfa->trie->nodes[id].child[2]); // Flag leaf (MSB).

   return newid;
}

size_t
trie_newnode
(
 trie_t ** triep,
 size_t parent
 )
{
   trie_t * trie = *triep;

   // Check trie size.
   if (trie->pos >= trie->size) {
      size_t newsize = trie->size * 2;
      *triep = trie = realloc(trie, sizeof(trie_t) + newsize * sizeof(node_t));
      if (trie == NULL) return (size_t)-1;
      // Initialize new nodes.
      trie->size = newsize;
   }
      
   // Consume one node of the trie.
   size_t newid = trie->pos;
   trie->nodes[newid].parent = parent;
   trie->nodes[newid].child[0] = trie->nodes[newid].child[1] = trie->nodes[newid].child[2] = 0;
   trie->pos++;

   // Go one level deeper.
   return newid;
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

unsigned char *
trie_encode
(
 const unsigned char * path,
 size_t nelements
)
{
   static const unsigned char power[5] = {81,27,9,3,1};
   // Allocate data.
   unsigned char * data = calloc(nelements/5 + (nelements%5 > 0), sizeof(char));
   // Convert ternary alphabet to binary. (5 ternary symbols per byte)
   for (size_t i = 0; i < nelements; i++) data[i/5] += path[i] * power[i%5];
   // Return.
   return data;
}

unsigned char *
trie_decode
(
 const unsigned char * data,
 size_t nelements
)
{
   static const unsigned char power[5] = {81,27,9,3,1};
   // Allocate path.
   unsigned char * path = malloc(nelements);
   unsigned char tmp = 0;
   // Convert binary to ternary alphabet. (One byte yields 5 ternary symbols)
   for (size_t i = 0; i < nelements; i++) {
      if (i%5 == 0) tmp = data[i/5];
      path[i] = tmp / power[i%5];
      tmp = tmp % power[i%5];
   }
   // Return.
   return path;
}


int
trie_compare
(
 const unsigned char * path,
 const unsigned char * data,
 size_t nelements
)
{
   static const unsigned char power[5] = {81,27,9,3,1};
   unsigned char tmp = data[0];
   // Direct comparion of ternary symbols with its binary representation.
   for (size_t i = 0; i < nelements; i++) {
      if (i % 5 == 0) tmp = data[i/5];
      if (tmp / power[i%5] != path[i]) return 0;
      tmp %= power[i%5];
   }
   return 1;
}
