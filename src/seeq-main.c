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
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include <stdio.h>
#include <getopt.h>

void say_usage(void);
void say_version(void);
void say_help(void);
void SIGSEGV_handler(int) __attribute__ ((noreturn));

static const char *USAGE = "Usage:"
"  seeq [options] pattern inputfile\n"
"\n   MATCHING OPTIONS:\n"
"    -d --distance [#]    maximum Levenshtein distance [default 0]\n"
"    -i --invert          return only the non-matching lines\n"
"    -b --best            scan the whole line to find the best match [default: first match only]\n"
"    -a --all             returns all the matches (implies -m) [default: first match only]\n"
"    -x --nondna [0,1,2]  non-DNA characters: 0-skip line, 1-convert to 'N', 2-ignore. [default 0]\n"
"\n   FORMAT OPTIONS:\n"
"    -c --count           returns the count of matching lines\n"
"    -m --match-only      print only the matched sequence\n"
"    -n --no-printline    do not print the matched sequence\n"
"    -l --lines           shows the line number of the match\n"
"    -p --positions       shows the position of the match\n"
"    -k --print-dist      shows the Levenshtein distance of the match\n"
"    -f --compact         prints output in compact format (line:pos:dist)\n"
"    -e --end             print only the end of the line, starting after the match\n"
"    -r --prefix          print only the prefix, ending before the match\n"
"    -s --split           print prefix match and suffix separated by tabs\n"
"\n   OTHER OPTIONS:\n"
"    -v --version         print version\n"
"    -y --memory          set DFA memory limit (in MB)\n"
"    -z --verbose         verbose using stderr\n";


void say_usage(void) { fprintf(stderr, "%s\n", USAGE); }
void say_version(void) { fprintf(stderr, SEEQ_VERSION "\n"); }
void say_help(void) { fprintf(stderr, "use '-h' for help.\n"); }

void SIGSEGV_handler(int sig) {
   void *array[10];
   int size;

   // get void*'s for all entries on the stack
   size = backtrace(array, 10);

   // print out all the frames to stderr
   fprintf(stderr, "Error: signal %d:\n", sig);
   backtrace_symbols_fd(array, size, STDERR_FILENO);
   exit(1);
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
   int split_flag     = -1;
   int best_flag      = -1;
   int nondna_flag    = -1;
   int memory_flag    = -1;
   int all_flag       = -1;

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
         {"print-dist",    no_argument, 0, 'k'},
         {"nondna",  required_argument, 0, 'x'},
         {"lines",         no_argument, 0, 'l'},
         {"count",         no_argument, 0, 'c'},
         {"invert",        no_argument, 0, 'i'},
         {"format-compact",no_argument, 0, 'f'},
         {"verbose",       no_argument, 0, 'z'},
         {"version",       no_argument, 0, 'v'},
         {"help",          no_argument, 0, 'h'},
         {"end",           no_argument, 0, 'e'},
         {"prefix",        no_argument, 0, 'r'},
         {"split",         no_argument, 0, 's'},
         {"best",          no_argument, 0, 'b'},
         {"all",           no_argument, 0, 'a'},
         {"memory",  required_argument, 0, 'y'},
         {"distance",required_argument, 0, 'd'},
         {0, 0, 0, 0}
      };

      c = getopt_long(argc, argv, "apmnilczfvkhersby:d:x:",
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

      case 'y':
         if (memory_flag < 0) {
            int memory = atoi(optarg);
            if (memory < 0) {
               say_version();
               fprintf(stderr, "error: memory limit must be a positive integer.\n");
               say_help();
               return EXIT_FAILURE;
            }
            memory_flag = atoi(optarg);
         }
         else {
            say_version();
            fprintf(stderr, "error: memory option set more than once.\n");
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

      case 'r':
         if (prefix_flag < 0) {
            prefix_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: 'prefix' option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'a':
         if (all_flag < 0) {
            all_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: 'all' option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;

      case 'x':
         if (nondna_flag < 0) {
            int nondna = atoi(optarg);
            if (nondna < 0 || nondna > 2) {
               say_version();
               fprintf(stderr, "error: nondna value must be either 0, 1 or 2.\n");
               say_help();
               return EXIT_FAILURE;
            }               
            nondna_flag = nondna;
         }
         else {
            say_version();
            fprintf(stderr, "error: 'nondna' option set more than once.\n");
            say_help();
            return EXIT_FAILURE;
         }
         break;


      case 'b':
         if (best_flag < 0) {
            best_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: 'best' option set more than once.\n");
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

      case 's':
         if (split_flag < 0) {
            split_flag = 1;
         }
         else {
            say_version();
            fprintf(stderr, "error: split option set more than once.\n");
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

      case 'k':
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

      default:
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
   if (split_flag == -1) split_flag = 0;
   if (best_flag == -1) best_flag = 0;
   if (nondna_flag == -1) nondna_flag = 0;
   if (memory_flag == -1) memory_flag = 0;
   if (all_flag == -1) all_flag = 0;
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
   args.split     = split_flag * maskinv;
   args.invert    = invert_flag * maskcnt;
   args.best      = best_flag * maskinv;
   args.non_dna    = nondna_flag;
   args.all       = all_flag;
   args.memory    = (size_t)memory_flag * 1024*1024;
   return seeq(expr, input, args);
}


