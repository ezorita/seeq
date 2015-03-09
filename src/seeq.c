/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
**
** Last modified: March 4, 2015
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
   seeq_t * sq =  seeqNew(expression, tau);
   if (sq == NULL) {
      fprintf(stderr, "error in 'seeqNew()'; %s\n:", seeqPrintError());
      return EXIT_FAILURE;
   }

   seeqfile_t * sqfile = seeqOpen(input);
   if (sqfile == NULL) {
      fprintf(stderr, "error in 'seeqOpen()': %s\n", seeqPrintError());
      return EXIT_FAILURE;
   }


   clock_t clk = 0;
   if (verbose) {
      fprintf(stderr, "\nmatching...\n");
      clk = clock();
   }

   if (args.count) {
      int retval = seeqFileMatch(sqfile, sq, SQ_COUNTLINES);
      if (retval < 0) fprintf(stderr, "error in 'seeqFileMatch()': %s\n", seeqPrintError());
      else fprintf(stdout, "%ld\n", retval);
   } else {
      int match_options = 0;
      if (args.invert) match_options = SQ_NOMATCH;
      else match_options = SQ_MATCH;
      if (args.best) match_options |= SQ_BEST;

      int retval;
      while ((retval = seeqFileMatch(sqfile, sq, match_options)) > 0) {
         if (args.compact) fprintf(stdout, "%ld:%d-%d:%d",sq->match.line, sq->match.start, sq->match.end-1, sq->match.dist);
         else {
            if (args.showline) fprintf(stdout, "%ld ", sq->match.line);
            if (args.showpos)  fprintf(stdout, "%d-%d ", sq->match.start, sq->match.end-1);
            if (args.showdist) fprintf(stdout, "%d ", sq->match.dist);
            if (args.matchonly) {
               sq->match.string[sq->match.end] = 0;
               fprintf(stdout, "%s", sq->match.string + sq->match.start);
            } else if (args.prefix) {
               sq->match.string[sq->match.start] = 0;
               fprintf(stdout, "%s", sq->match.string);
            } else if (args.endline) {
               fprintf(stdout, "%s", sq->match.string + sq->match.end);
            } else if (args.printline) {
               if (COLOR_TERMINAL && isatty(fileno(stdout))) {
                  // Prefix.
                  char tmp = sq->match.string[sq->match.start];
                  sq->match.string[sq->match.start] = 0;
                  fprintf(stdout, "%s", sq->match.string);
                  sq->match.string[sq->match.start] = tmp;
                  // Color match.
                  fprintf(stdout, (sq->match.dist ? BOLDRED : BOLDGREEN));
                  tmp = sq->match.string[sq->match.end];
                  sq->match.string[sq->match.end] = 0;
                  fprintf(stdout, "%s" RESET, sq->match.string + sq->match.start);
                  sq->match.string[sq->match.end] = tmp;
                  fprintf(stdout, "%s", sq->match.string + sq->match.end);
               }
               else fprintf(stdout, "%s", sq->match.string);
            }
         }
         fprintf(stdout, "\n");
      }
      if (retval == -1) {
         fprintf(stderr, "error in 'seeqFileMatch()': %s\n", seeqPrintError());
      }
   }
   
   if (verbose) {
      fprintf(stderr, "done in %.3fs\n", (clock()-clk)*1.0/CLOCKS_PER_SEC);
   }
   
   seeqClose(sqfile);
   seeqFree(sq);

   return EXIT_SUCCESS;
}

