#include "seeq.h"

char *USAGE = "Usage:\n"
"  seeq [options] (-i patternfile | pattern) inputfile\n"
"    -v --verbose             verbose\n"
"    -i --input <file>        match using multiple patterns from a file\n"
"    -d --distance            maximum Levenshtein distance (default 0)\n"
"    -t --threads             maximum number of threads\n"
"    -c --count               show only match count\n"
"    -r --reverse-complement  match also the reverse complements\n"
"    -m --match-only          print only the matching string\n"
"    -n --no-printline        does not print the matching line\n"
"    -l --show-line           prints the line number of the match\n"
"    -p --show-position       shows the position of the match within the matched line\n"
"    -s --show-dist           prints the Levenshtein distance of the match\n"
"    -f --compact             prints output in compact format\n"
"    -e --line-end            print only the end of the line starting after the match";



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
   //signal(SIGSEGV, SIGSEGV_handler); 

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
   int input_flag     = -1;
   int reverse_flag   = -1;
   int threads_flag   = -1;

   // Unset options (value 'UNSET').
   char *input = NULL;
   char *expfile = NULL;
   char *expr_flag = NULL;

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
         {"line-end",      no_argument, 0, 'e'},
         {"reverse",       no_argument, 0, 'r'},
         {"threads" ,required_argument, 0, 't'},         
         {"input"   ,required_argument, 0, 'i'},         
         {"distance",required_argument, 0, 'd'},
         {0, 0, 0, 0}
      };

      c = getopt_long(argc, argv, "pmnslcfvhert:i:d:",
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

      case 'i':
         if (input_flag < 0) {
            input_flag = 1;
            expfile = optarg;
         }
         else {
            fprintf(stderr, "pattern file option set more than once\n");
            say_usage();
            return 1;
         }
         break;

      case 't':
         if (threads_flag < 0) {
            threads_flag = atoi(optarg);
         }
         else {
            fprintf(stderr, "threads option set more than once\n");
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

      case 'r':
         if (reverse_flag < 0) {
            reverse_flag = 1;
         }
         else {
            fprintf(stderr, "reverse option set more than once\n");
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

   if (input_flag == -1)
      expr_flag = argv[optind++];

   if (optind < argc) {
      if (optind == argc-1 && input == NULL) {
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
   if (input_flag == -1) input_flag = 0;
   if (reverse_flag == -1) reverse_flag = 0;
   if (threads_flag == -1) threads_flag = 1;

   if (!showdist_flag && !showpos_flag && !printline_flag && !matchonly_flag && !showline_flag && !count_flag && !compact_flag) {
      fprintf(stderr, "Invalid options: No output will be generated.\n");
      exit(1);
   }
   
   // Options variable.
   int options = 0;
   options |= showdist_flag;
   options |= showline_flag   << 1;
   options |= printline_flag  << 2;
   options |= count_flag      << 3;
   options |= verbose_flag    << 4;
   options |= precompute_flag << 5;
   options |= endline_flag    << 6;
   // These 3 must be the greatest (RDFA conditional).
   options |= compact_flag    << 8;
   options |= showpos_flag    << 9;
   options |= matchonly_flag  << 10;

   // mmap file.
   if (verbose_flag) fprintf(stderr, "loading input file\n");
   int fdi = open(input, O_RDWR);
   if (fdi == -1) {
      fprintf(stderr,"error: could not open file: %s\n\n", strerror(errno));
      exit(1);
   }
   unsigned long isize = lseek(fdi, 0, SEEK_END);
   lseek(fdi, 0, SEEK_SET);
   // Map file to memory.
   char * data = (char *) mmap(NULL, isize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fdi, 0);
   if (data == MAP_FAILED) {
      fprintf(stderr, "error loading data into memory (mmap): %s\n", strerror(errno));
      exit(EXIT_FAILURE);
   }

   // Expression vector.
   int nexpr;
   char ** expr;

   // Create expression vector.
   if (input_flag) {
      // Read expression file
      expr = read_expr_file(expfile, &nexpr);
   } else {
      int len = strlen(expr_flag);
      char * pattern = malloc(len+1);
      strcpy(pattern, expr_flag);
      expr = malloc(sizeof(char *));
      expr[0] = pattern;
      nexpr = 1;
   }
   
   if (reverse_flag) {
      expr = realloc(expr, 2*nexpr * sizeof(char *));
      if (expr == NULL) {
         fprintf(stderr, "error (realloc) in 'main:reverse_flag': %s\n", strerror(errno));
         exit(1);
      }
      for (int i = 0; i < nexpr; i++) expr[nexpr + i] = reverse_pattern(expr[i]);
      nexpr *= 2;
   }

   // SCHEDULER: Multithreaded pipeline.
   pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;
   mtcont_t control = {.mutex = &mutex, .cond = &cond, .nthreads = 0 };

   // Bridge stacks.
   sstack_t ** pipein = NULL;
   sstack_t ** pipebuf = malloc((nexpr-1) * sizeof(sstack_t *));
   for (int i = 0; i < nexpr; i++) {
      // Bridge stacks.
      sstack_t ** pipeout;
      if (i < nexpr - 1) {
         pipebuf[i] = new_sstack(INITIAL_STACK_SIZE);
         pipeout = pipebuf + i;
      } else {
         pipeout = NULL;
      }
      seeqarg_t * args = malloc(sizeof(seeqarg_t));
      args->options = options;
      args->dist    = dist_flag;
      args->expr    = expr[i];
      args->data    = data;
      args->isize   = isize;
      args->stckin  = pipein;
      args->stckout = pipeout;
      args->control = &control;

      // Connect next node.
      pipein = pipeout;

      // Create new thread.
      pthread_t newthread;
      pthread_mutex_lock(&mutex);
      control.nthreads++;
      pthread_create(&newthread, NULL, seeq, (void *) args);
      pthread_detach(newthread);

      // Monitor.
      while (control.nthreads == threads_flag) {
         pthread_cond_wait(&cond, &mutex);
      }
      pthread_mutex_unlock(&mutex);
   }

   pthread_mutex_lock(&mutex);
   while (control.nthreads > 0) {
      pthread_cond_wait(&cond, &mutex);
   }
   pthread_mutex_unlock(&mutex);


   return 0;
}
