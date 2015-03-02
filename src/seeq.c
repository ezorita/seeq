/*
** Copyright 2015 Eduard Valera Zorita.
**
** File authors:
**  Eduard Valera Zorita (eduardvalera@gmail.com)
**
** Last modified: March 2, 2015
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
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

char *USAGE = "Usage:"
"  seeq [options] pattern inputfile\n"
"    -v --version         print version\n"
"    -z --verbose         verbose using stderr\n"
"    -d --distance        maximum Levenshtein distance (default 0)\n"
"    -c --count           returns the count of matching lines\n"
"    -i --invert          return only the non-matching lines\n"
"    -m --match-only      print only the matched part of the matched lines\n"
"    -n --no-printline    does not print the matching line\n"
"    -l --lines           prints the original line number of the match\n"
"    -p --positions       prints the position of the match within the matched line\n"
"    -s --print-dist      prints the Levenshtein distance wrt the pattern\n"
"    -f --compact         prints output in compact format (line:pos:dist)\n"
"    -e --end             print only the end of the line, starting after the match\n"
"    -b --prefix          print only the prefix, ending before the match\n";

struct seeqarg_t {
   int showdist;
   int showpos;
   int showline;
   int printline;
   int matchonly;
   int count;
   int compact;
   int dist;
   int verbose;
   int endline;
   int prefix;
   int invert;
};


void say_usage(void) { fprintf(stderr, "%s\n", USAGE); }
void say_version(void) { fprintf(stderr, VERSION "\n"); }
void say_help(void) { fprintf(stderr, "use '-h' for help.\n"); }

void SIGSEGV_handler(int sig) {
   void *array[10];
   size_t size;

   // get void*'s for all entries on the stack
   size = backtrace(array, 10);

   // print out all the frames to stderr
   fprintf(stderr, "Error: signal %d:\n", sig);
   backtrace_symbols_fd(array, size, STDERR_FILENO);
   exit(1);
}

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
   seeq_t * sq = seeqOpen(input, expression, tau);
   if (sq == NULL) {
      fprintf(stderr, "error in 'seeqOpen()': %s\n", strerror(errno));
      return EXIT_FAILURE;
   }

   clock_t clk = 0;
   if (verbose) {
      fprintf(stderr, "\nmatching...\n");
      clk = clock();
   }
   
   if (args.count) {
      fprintf(stdout, "%ld\n", seeqMatch(sq, SQ_COUNT));
   } else {
      int match_options = 0;
      if (args.invert) match_options = SQ_NOMATCH;
      else match_options = SQ_MATCH;

      while (seeqMatch(sq, match_options) > 0) {
         if (args.compact) fprintf(stdout, "%ld:%d-%d:%d\n",sq->match.line, sq->match.start, sq->match.end-1, sq->match.dist);
         else {
            if (args.showline) fprintf(stdout, "%ld ", sq->match.line);
            if (args.showpos)  fprintf(stdout, "%d-%d ", sq->match.start, sq->match.end);
            if (args.showdist) fprintf(stdout, "%d ", sq->match.dist);
            if (args.printline) fprintf(stdout, "%s\n", sq->match.string);
            else if (args.matchonly) {
               sq->match.string[sq->match.end] = 0;
               fprintf(stdout, "%s\n", sq->match.string + sq->match.start);
            } else if (args.prefix) {
               sq->match.string[sq->match.start] = 0;
               fprintf(stdout, "%s\n", sq->match.string);
            } else if (args.endline) {
               fprintf(stdout, "%s\n", sq->match.string + sq->match.end);
            }
         }
      }
   }
   
   if (verbose) {
      fprintf(stderr, "done in %.3fs\n", (clock()-clk)*1.0/CLOCKS_PER_SEC);
   }
   
   seeqClose(sq);

   return EXIT_SUCCESS;
}


int
main
(
   int argc,
   char **argv
)
{
   // Backtrace handler
   signal(SIGSEGV, SIGSEGV_handler); 
   char *expr, *input;

   // Unset flags (value -1).
   int showdist_flag  = -1;
   int showpos_flag   = -1;
   int printline_flag = -1;
   int matchonly_flag = -1;
   int showline_flag  = -1;
   int count_flag     = -1;
   int invert_flag    = -1;
   int compact_flag   = -1;
   int dist_flag      = -1;
   int verbose_flag   = -1;
   int endline_flag   = -1;
   int prefix_flag    = -1;

   // Unset options (value 'UNSET').
   input = NULL;

   if (argc == 1) {
      say_version();
      say_usage();
      return EXIT_SUCCESS;
   }

   int c;
   while (1) {
      int option_index = 0;
      static struct option long_options[] = {
         {"positions",     no_argument, 0, 'p'},
         {"match-only",    no_argument, 0, 'm'},
         {"no-printline",  no_argument, 0, 'n'},
         {"print-dist",    no_argument, 0, 's'},
         {"lines",         no_argument, 0, 'l'},
         {"count",         no_argument, 0, 'c'},
         {"invert",        no_argument, 0, 'i'},
         {"format-compact",no_argument, 0, 'f'},
         {"verbose",       no_argument, 0, 'z'},
         {"version",       no_argument, 0, 'v'},
         {"help",          no_argument, 0, 'h'},
         {"end",           no_argument, 0, 'e'},         
         {"prefix",        no_argument, 0, 'b'},                  
         {"distance",required_argument, 0, 'd'},
         {0, 0, 0, 0}
      };

      c = getopt_long(argc, argv, "pmnislczfvhebd:",
            long_options, &option_index);
 
      /* Detect the end of the options. */
      if (c == -1) break;
  
      switch (c) {
      case 'd':
         if (dist_flag < 0) {
            int dist = atoi(optarg);
            if (dist < 0) {
               say_version();
               fprintf(stderr, "error: distance must be a positive integer.\n");
               say_help();
               return EXIT_FAILURE;
            }
            dist_flag = atoi(optarg);
         }
         else {
            say_version();
            fprintf(stderr, "error: distance option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'v':
         say_version();
         return EXIT_SUCCESS;

      case 'z':
         if (verbose_flag < 0) {
            verbose_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: verbose option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;



      case 'b':
         if (prefix_flag < 0) {
            prefix_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: prefix option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;


      case 'e':
         if (endline_flag < 0) {
            endline_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: line-end option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'p':
         if (showpos_flag < 0) {
            showpos_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: show-position option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'm':
         if (matchonly_flag < 0) {
            matchonly_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: match-only option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'n':
         if (printline_flag < 0) {
            printline_flag = 0;
         }
         else {
            say_version();
            fprintf(stderr, "error: no-printline option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 's':
         if (showdist_flag < 0) {
            showdist_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: show-distance option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'l':
         if (showline_flag < 0) {
            showline_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: show-line option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'c':
         if (count_flag < 0) {
            count_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: count option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'i':
         if (invert_flag < 0) {
            invert_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: invert option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'f':
         if (compact_flag < 0) {
            compact_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: format-compact option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'h':
         say_version();
         say_usage();
         exit(0);
         break;
      }
   }

   if (optind == argc) {
      say_version();
      fprintf(stderr, "error: not enough arguments.\n");
      say_help();
      return EXIT_FAILURE;
   }
   expr = argv[optind++];

   if (optind < argc) {
      if ((optind == argc - 1) && (input == NULL)) {
         input = argv[optind];
      }
      else {
         say_version();
         fprintf(stderr, "error: too many options.\n");
         say_help();
         return EXIT_FAILURE;
      }
   }
   if (count_flag == -1) count_flag = 0;
   if (showdist_flag == -1) showdist_flag = 0;
   if (showpos_flag  == -1) showpos_flag = 0;
   if (matchonly_flag == -1) matchonly_flag = 0;
   if (showline_flag == -1) showline_flag = 0;
   if (invert_flag == -1) invert_flag = 0;
   if (compact_flag == -1) compact_flag = 0;
   if (dist_flag == -1) dist_flag = 0;
   if (verbose_flag == -1) verbose_flag = 0;
   if (endline_flag == -1) endline_flag = 0;
   if (prefix_flag == -1) prefix_flag = 0;
   if (printline_flag == -1) printline_flag = (!matchonly_flag && !endline_flag && !prefix_flag);

   if (!showdist_flag && !showpos_flag && !printline_flag && !matchonly_flag && !showline_flag && !count_flag && !compact_flag && !prefix_flag && !endline_flag) {
      say_version();
      fprintf(stderr, "Invalid options: No output will be generated.\n");
      say_help();
      return EXIT_FAILURE;
   }

   int maskcnt = !count_flag;
   int maskinv = !invert_flag * maskcnt;

   struct seeqarg_t args;

   args.showdist  = showdist_flag * maskinv;
   args.showpos   = showpos_flag * maskinv;
   args.showline  = showline_flag * maskcnt;
   args.printline = printline_flag * maskinv;
   args.matchonly = matchonly_flag * maskinv;
   args.count     = count_flag;
   args.compact   = compact_flag * maskinv;
   args.dist      = dist_flag;
   args.verbose   = verbose_flag;
   args.endline   = endline_flag * maskinv;
   args.prefix    = prefix_flag * maskinv;
   args.invert    =invert_flag * maskcnt;

   return seeq(expr, input, args);
}


