#include "seeq.h"

char *USAGE = "Usage:\n"
"  seeq [options] pattern inputfile\n"
"    -v --verbose         verbose\n"
"    -d --distance        maximum Levenshtein distance (default 0)\n"
"    -c --count           show only match count\n"
"    -m --match-only      print only the matching string\n"
"    -n --no-printline    does not print the matching line\n"
"    -l --show-line       prints the original line of the match\n"
"    -p --show-position   shows the position of the match within the matched line\n"
"    -s --show-dist       prints the Levenshtein distance of each match\n"
"    -f --compact         prints output in compact format\n"
"    -e --line-end        print the end of the line starting after the match\n"
"    -r --precompute      precomputes DFA before matching";


void say_usage(void) { fprintf(stderr, "%s\n", USAGE); }

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
main(
   int argc,
   char **argv
)
{
   // Backtrace handler
   signal(SIGSEGV, SIGSEGV_handler); 

   // Unset flags (value -1).
   int showdist_flag  = -1;
   int showpos_flag   = -1;
   int printline_flag = -1;
   int matchonly_flag = -1;
   int showline_flag  = -1;
   int count_flag     = -1;
   int compact_flag   = -1;
   int dist_flag      = -1;
   int verbose_flag   = -1;
   int precompute_flag = -1;
   int endline_flag   = -1;

   // Unset options (value 'UNSET').
   char *input = NULL;

   int c;
   while (1) {
      int option_index = 0;
      static struct option long_options[] = {
         {"show-position", no_argument, 0, 'p'},
         {"match-only",    no_argument, 0, 'm'},
         {"no-printline",  no_argument, 0, 'n'},
         {"show-dist",     no_argument, 0, 's'},
         {"show-line",     no_argument, 0, 'l'},
         {"count",         no_argument, 0, 'c'},
         {"format-compact",no_argument, 0, 'f'},
         {"verbose",       no_argument, 0, 'v'},
         {"help",          no_argument, 0, 'h'},
         {"precompute",    no_argument, 0, 'r'},
         {"line-end",      no_argument, 0, 'e'},         
         {"distance",required_argument, 0, 'd'},
         {0, 0, 0, 0}
      };

      c = getopt_long(argc, argv, "pmnslcfvhred:",
            long_options, &option_index);
 
      /* Detect the end of the options. */
      if (c == -1) break;
  
      switch (c) {
      case 'd':
         if (dist_flag < 0) {
            dist_flag = atoi(optarg);
         }
         else {
            fprintf(stderr, "distance option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'v':
         if (verbose_flag < 0) {
            verbose_flag = 1;
         }
         else {
            fprintf(stderr, "verbose option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'e':
         if (endline_flag < 0) {
            endline_flag = 1;
         }
         else {
            fprintf(stderr, "line-end option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'r':
         if (precompute_flag < 0) {
            precompute_flag = 1;
         }
         else {
            fprintf(stderr, "precompute option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'p':
         if (showpos_flag < 0) {
            showpos_flag = 1;
         }
         else {
            fprintf(stderr, "show-position option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'm':
         if (matchonly_flag < 0) {
            matchonly_flag = 1;
         }
         else {
            fprintf(stderr, "match-only option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'n':
         if (printline_flag < 0) {
            printline_flag = 0;
         }
         else {
            fprintf(stderr, "no-printline option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 's':
         if (showdist_flag < 0) {
            showdist_flag = 1;
         }
         else {
            fprintf(stderr, "show-distance option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'l':
         if (showline_flag < 0) {
            showline_flag = 1;
         }
         else {
            fprintf(stderr, "show-line option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'c':
         if (count_flag < 0) {
            count_flag = 1;
         }
         else {
            fprintf(stderr, "count option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'f':
         if (compact_flag < 0) {
            compact_flag = 1;
         }
         else {
            fprintf(stderr, "format-compact option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 'h':
         say_usage();
         exit(0);
         break;
      }
   }

   char * expr = argv[optind++];

   if (optind < argc) {
      if ((optind == argc - 1) && (input == NULL)) {
         input = argv[optind];
      }
      else {
         fprintf(stderr, "too many options\n");
         say_usage();
         return 1;
      }
   }
   if (showdist_flag == -1) showdist_flag = 0;
   if (showpos_flag  == -1) showpos_flag = 0;
   if (printline_flag == -1) printline_flag = 1;
   if (matchonly_flag == -1) matchonly_flag = 0;
   if (showline_flag == -1) showline_flag = 0;
   if (count_flag == -1) count_flag = 0;
   if (compact_flag == -1) compact_flag = 0;
   if (dist_flag == -1) dist_flag = 0;
   if (verbose_flag == -1) verbose_flag = 0;
   if (precompute_flag == -1) precompute_flag = 0;
   if (endline_flag == -1) endline_flag = 0;

   if (!showdist_flag && !showpos_flag && !printline_flag && !matchonly_flag && !showline_flag && !count_flag && !compact_flag) {
      fprintf(stderr, "Invalid options: No output will be generated.\n");
      exit(1);
   }

   struct seeqarg_t args = {showdist_flag, showpos_flag,
                            showline_flag, printline_flag,
                            matchonly_flag, count_flag,
                            compact_flag, dist_flag,
                            verbose_flag, precompute_flag,
                            endline_flag};

   seeq(expr, input, args);
   return 0;
}
