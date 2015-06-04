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

#include "seeq.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


int
seeq
(
 char * expression,
 char * input,
 struct seeqarg_t args
)
// SYNOPSIS:                                                              
//   Sequentially searches for a given pattern in the file name
//   passed in "input", or from the standard input if the input argu-
//   ment is set to NULL. The output generated is sent to the standard
//   output and depends on the flags enabled in the 'args' struct.
//   The matching metric is the Levenshtein distance. The input sequences
//   must be a succession of DNA/RNA nucleotides ('A','C','T','G','U','N')
//   separated by newline characters '\n'.
//                                                                        
// PARAMETERS:                                                            
//   expression: the pattern to match.
//   input: string containing the filename or NULL pointer for stdin.
//   args: properly filled seeqarg_t struct to customize the output format.
//     struct seeqarg_t:
//     - showdist: Prints the levenshtein distance of the match.
//     - showpos: Prints the position (stard-end) of the matched nucleotides.
//     - showline: Prints the line number of the matched line.
//     - printline: Prints the matched line.
//     - matchonly: Prints only the matched nucleotides.
//     - count: The only output is the count of matched lines.
//     - compact: Prints matches in compact format.
//     - dist: Match distance threshold.
//     - verbose: Print verbose messages.
//     - endline: Prints only the end of the line starting after the match.
//     - prefix: Prints only the beginnig of the line ending before the match.
//     - invert: Prints only the non-matched lines.
//     ** All format options are enabled setting its value to 1, except dist,
//     ** which must contain a positive integer value.
//                                                                        
// RETURN:                                                                
//   seeq returns 0 on success, or -1 if an error occurred.
//
// SIDE EFFECTS:
//   None.
{
   const int verbose = args.verbose;
   const int tau = args.dist;

   if (verbose) fprintf(stderr, "opening input file... ");
   seeq_t * sq =  seeqNew(expression, tau, args.memory);
   if (sq == NULL) {
      fprintf(stderr, "error in 'seeqNew()'; %s\n:", seeqPrintError());
      return EXIT_FAILURE;
   }

   seeqfile_t * sqfile = seeqOpen(input);
   if (sqfile == NULL) {
      fprintf(stderr, "error in 'seeqOpen()': %s\n", seeqPrintError());
      seeqFree(sq);
      return EXIT_FAILURE;
   }


   clock_t clk = 0;
   if (verbose) {
      fprintf(stderr, "\nmatching...\n");
      clk = clock();
   }

   int match_options = 0;
   if (args.non_dna == 1) match_options |= SQ_CONVERT;
   else if (args.non_dna == 2) match_options |= SQ_IGNORE;

   if (args.count) {
      long retval = seeqFileMatch(sqfile, sq, match_options, SQ_COUNTLINES);
      if (retval < 0) fprintf(stderr, "error in 'seeqFileMatch()': %s\n", seeqPrintError());
      else fprintf(stdout, "%ld\n", retval);
   } else {
      if (args.all) {
         match_options |= SQ_ALL;
         args.matchonly = 1;
      }
      else if (args.best) match_options |= SQ_BEST;


      long retval = 0;
      if (args.invert) {
         while ((retval = seeqFileMatch(sqfile, sq, match_options, SQ_NOMATCH)) > 0) {
            if (args.showline) fprintf(stdout, "%ld ", sqfile->line);
            fprintf(stdout, "%s\n", sq->string);
         }
      } else {
         while ((retval = seeqFileMatch(sqfile, sq, match_options, SQ_MATCH)) > 0) {
            match_t * match;
            while((match = seeqMatchIter(sq)) != NULL) {
               if (args.compact) fprintf(stdout, "%ld:%ld-%ld:%ld",sqfile->line, match->start, match->end-1, match->dist);
               else {
                  if (args.showline) fprintf(stdout, "%ld ", sqfile->line);
                  if (args.showpos)  fprintf(stdout, "%ld-%ld ", match->start, match->end-1);
                  if (args.showdist) fprintf(stdout, "%ld ", match->dist);
                  if (args.matchonly) {
                     char tmp = sq->string[match->end];
                     sq->string[match->end] = 0;
                     fprintf(stdout, "%s", sq->string + match->start);
                     sq->string[match->end] = tmp;
                  } else if (args.prefix) {
                     sq->string[match->start] = 0;
                     fprintf(stdout, "%s", sq->string);
                  } else if (args.endline) {
                     fprintf(stdout, "%s", sq->string + match->end);
                  } else if (args.printline) {
                     if (COLOR_TERMINAL && isatty(fileno(stdout))) {
                        // Prefix.
                        char tmp = sq->string[match->start];
                        sq->string[match->start] = 0;
                        fprintf(stdout, "%s", sq->string);
                        sq->string[match->start] = tmp;
                        // Color match.
                        fprintf(stdout, (match->dist ? BOLDRED : BOLDGREEN));
                        tmp = sq->string[match->end];
                        sq->string[match->end] = 0;
                        fprintf(stdout, "%s" RESET, sq->string + match->start);
                        sq->string[match->end] = tmp;
                        fprintf(stdout, "%s", sq->string + match->end);
                     }
                     else fprintf(stdout, "%s", sq->string);
                  }
               }
               fprintf(stdout, "\n");
            }
         }
      }
      if (retval == -1) {
         fprintf(stderr, "error in 'seeqFileMatch()': %s\n", seeqPrintError());
      }
   }
   
   if (verbose) {
      size_t * data = (size_t *) sq->dfa;
      size_t mem_dfa  = *data * ((6*4) + strlen(expression)/5 + (strlen(expression)%5 > 0));
      size_t mem_trie = *(size_t *)(*(data + 4)) * 16;
      data = (size_t *) sq->rdfa;
      size_t mem_rdfa  = *data * ((6*4) + strlen(expression)/5 + (strlen(expression)%5 > 0));
      size_t mem_rtrie = *(size_t *)(*(data + 4)) * 16;
      double mb = 1024.0*1024.0;
      fprintf(stderr, "memory: %.2f MB (DFA: %.2f MB, trie: %.2f MB)\n", (mem_dfa + mem_trie + mem_rdfa + mem_rtrie)/mb, (mem_dfa+mem_rdfa)/mb, (mem_trie+mem_rtrie)/mb);
      fprintf(stderr, "done in %.3fs\n", (clock()-clk)*1.0/CLOCKS_PER_SEC);
   }
   
   seeqClose(sqfile);
   seeqFree(sq);

   return EXIT_SUCCESS;
}

seeqfile_t *
seeqOpen
(
 const char * file
)
// SYNOPSIS:                                                              
//   Creates a seeqfile_t structure to match a file directly against a pattern.
//   If 'file' is set to NULL, the lines will be read from 'stdin'. The returned
//   structure must be passed to 'seeqFileMatch'.
//                                                                        
// PARAMETERS:                                                            
//   file       : name of the file to match. Set to NULL to read from stdin.
//
// RETURN:                                                                
//   Returns a pointer to a seeqfile_t structure or NULL in case of error, and seeqerr
//   is set appropriately.
//
// SIDE EFFECTS:
//   The returned seeqfile_t structure must be safely freed using 'seeqClose'.
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
//   Closes the file (if any) and safely frees a seeqfile_t structure.
//                                                                        
// PARAMETERS:                                                            
//   sqfile : a pointer to an allocated seeqfile_t structure.
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
 int          match_opt,
 int          file_opt
)
// SYNOPSIS:                                                              
//   Finds the next match in the file pointed by 'sqfile', starting from the position where
//   the previous call to 'seeqFileMatch' returned. The matching pattern and distance are
//   determined by the seeq_t structure. The matching behavior can be specified in 'options'
//   using the defined SQ_ macros.
//                                                                        
// PARAMETERS:                                                            
//   sqfile   : pointer to a seeqfile_t structure obtained with 'seeqOpen'.
//   sq       : pointer to a seeq_t structure obtained with 'seeqNew'.
//   match_opt: matching options. (see 'seeqStringMatch').
//   file_opt : file matching options.
//              * SQ_ANY         At the end of each line.
//              * SQ_MATCH       After a matching line is found.
//              * SQ_NOMATCH     After a non-matching line is found.
//              * SQ_COUNTLINES  Processes the whole file and returns the number of matching 
//                               lines.
//              * SQ_COUNTMATCH  Processes the whole file and returns the count of matching
//                               positions.
//
// RETURN:                                                                
//   If SQ_COUNTLINES is set, returns the number of matching lines.
//   If SQ_COUNTMATCH is set, returns the total number of matches in the file.
//   If SQ_MATCH is set, returns 1 if a match is found before EOF, 0 otherwise.
//   If SQ_BEST is set, returns the number of matches found in the line or 0 if EOF is
//   reached before finding any match.
//   If SQ_NOMATCH is set, returns 1 if a non-matching line is found before EOF, 0 otherwise.
//   If SQ_ANY is set, reads one line and returns 1.
//
//   Returns 0 when the end of the file has been reached. In case of error, -1 is returned and
//   seeqerr is set appropriately.
//
//   Whenever this function returns 1, the information about the match can be read by passing
//   the 'seeq_t' structure to the 'seeqGet*' functions. The non-matching lines are indicated
//   with a matching distance of -1 (this can be checked either using 'seeqGetDistance()' or
//   directly reading sqfile->match.dist).
//
// SIDE EFFECTS:
//   The match details of the last match stored in 'sq' overriden and the file pointer offset
//   in 'seeqfile' is updated to the new matching position.
{
   // Set error to 0.
   seeqerr = 0;

   // Replace match options.
   if (file_opt == SQ_COUNTMATCH) match_opt = (match_opt & ~MASK_MATCH) | SQ_ALL;
   else if (file_opt == SQ_COUNTLINES) match_opt = (match_opt & ~MASK_MATCH) | SQ_FIRST;

   if (sqfile->fdi == NULL) {
      seeqerr = 10;
      return -1;
   }

   // Aux vars.
   long count = 0;
   size_t startline = sqfile->line;
   size_t bufsz = 0;
   ssize_t readsz;

   while ((readsz = getline(&(sq->string), &bufsz, sqfile->fdi)) > 0) {
      sqfile->line++;
      // Remove newline.
      char * data = sq->string;
      if (data[readsz-1] == '\n') data[--readsz] = 0;

      // Call String Match
      long rval = seeqStringMatch(data, sq, match_opt);
      if (rval == -1) return -1;
      else count += rval;

      // Break when match is found.
      if (file_opt == SQ_ANY || (count > 0 && (file_opt == SQ_MATCH)) || ((rval == 0) && (file_opt == SQ_NOMATCH)))
         return 1;
   }

   // If nothing was read, return 0.
   if (sqfile->line == startline) return 0;
   else return count;
}
