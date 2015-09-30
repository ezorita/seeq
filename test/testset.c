#include "libseeq.h"
#include "seeqcore.h"
#include "faultymalloc.h"
#include "seeq.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <execinfo.h>
#include <unistd.h>

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

// --  ERROR MESSAGE HANDLING FUNCTIONS  -- //

char ERROR_BUFFER[1024];
char OUTPUT_BUFFER[1024];
int BACKUP_FILE_DESCRIPTOR;
int BACKUP_STDOUT;
int tty_value = 0;

extern

int isatty(int fd);

int
isatty
(
int fd
)
{
   return tty_value;
}

void
mute_stderr
(void)
{
   fflush(stderr);
   int fd = open("/dev/null", O_WRONLY);
   dup2(fd, STDERR_FILENO);
}

void
unmute_stderr
(void)
{
   dup2(BACKUP_FILE_DESCRIPTOR, STDERR_FILENO);
   BACKUP_FILE_DESCRIPTOR = dup(STDERR_FILENO);
}

void
mute_stdout
(void)
{
   fflush(stderr);
   int fd = open("/dev/null", O_WRONLY);
   dup2(fd, STDOUT_FILENO);
}

void
redirect_stdout_to
(
   char buffer[]
)
{
   // Flush stderr, redirect to /dev/null and set buffer.
   fflush(stdout);
   int temp = open("/dev/null", O_WRONLY);
   dup2(temp, STDOUT_FILENO);
   memset(buffer, '\0', 1024 * sizeof(char));
   setvbuf(stdout, buffer, _IOFBF, 1024);
   close(temp);
   fflush(stdout);
}


void
redirect_stderr_to
(
   char buffer[]
)
{
   // Flush stderr, redirect to /dev/null and set buffer.
   fflush(stderr);
   int temp = open("/dev/null", O_WRONLY);
   dup2(temp, STDERR_FILENO);
   memset(buffer, '\0', 1024 * sizeof(char));
   setvbuf(stderr, buffer, _IOFBF, 1024);
   close(temp);
   // Fill the buffer (needed for reset).
   fprintf(stderr, "fill the buffer");
   fflush(stderr);
}

void
unredirect_sderr
(void)
{
   fflush(stderr);
   dup2(BACKUP_FILE_DESCRIPTOR, STDERR_FILENO);
   setvbuf(stderr, NULL, _IONBF, 0);
}

// --  TEST FUNCTIONS -- //

void
test_trie_new
(void)
{

   for (int i = 1 ; i < 100 ; i++) {
   for (int j = 1 ; j < 100 ; j++) {
      trie_t *trie = trie_new(i*100, j*100);
      g_assert(trie != NULL);
      g_assert_cmpint(trie->pos, ==, 1);
      g_assert_cmpint(trie->size, ==, i*100);
      g_assert_cmpint(trie->height, ==, j*100);
      for (int k = 0; k < TRIE_CHILDREN; k++)
         g_assert_cmpint(trie->nodes[0].child[k], ==, 0);
      g_assert_cmpint(trie->nodes[0].flags, ==, 0);
      free(trie);
   }
   }

   trie_t *trie = trie_new(0, 0);
   g_assert(trie != NULL);
   g_assert_cmpint(trie->pos, ==, 1);
   g_assert_cmpint(trie->size, ==, 1);
   g_assert_cmpint(trie->height, ==, 1);
   for (int k = 0; k < TRIE_CHILDREN; k++)
      g_assert_cmpint(trie->nodes[0].child[k], ==, 0);
   g_assert_cmpint(trie->nodes[0].flags, ==, 0);
   free(trie);

   // Alloc test.
   mute_stderr();
   set_alloc_failure_rate_to(1.1);
   trie_t *ret = trie_new(1, 10);
   reset_alloc();
   unmute_stderr();
   g_assert(ret == NULL);

   return;

}


void
test_trie_insert
(void)
{
   int trie_nodes = 1;
   int trie_height = 10;
   dfa_t * dfa = dfa_new(trie_height,0,100,trie_nodes,0);
   g_assert(dfa != NULL);
   
   // Test paths.
   uint8_t test_path[9][10] = {
      {2,1,1,1,1,1,1,1,1,1},
      {0,0,0,0,0,0,0,0,0,0},
      {0,0,0,1,0,0,0,0,0,0},
      {1,1,1,1,1,1,1,1,1,1},
      {1,0,1,0,1,0,1,0,1,0},
      {2,0,1,0,1,0,1,0,1,0},
      {2,1,1,1,1,1,1,1,1,0},
      {1,1,1,1,1,1,1,1,1,0},
      {1,0,0,0,0,1,2,0,3,0}};

   // Insert path references.
   for (int i = 0; i < 8; i++) {
      vertex_t * vertex = (vertex_t *) (dfa->states + (i+1) * dfa->state_size);
      path_encode(test_path[i], vertex->code, trie_height);
   }

   // Trie contains path 2,1,1,1,1,1...
   g_assert_cmpint(dfa->trie->pos, ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].child[0], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[0].child[1], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[0].child[2], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].flags, ==, 0b0100);

   int rval;
   // Insert leftmost branch in the trie.
   rval = trie_insert(dfa, test_path[1], 2); // UPDATE DFA NODES
   g_assert_cmpint(rval, ==, 0);
   g_assert_cmpint(dfa->trie->pos, ==, 1);
   g_assert_cmpint(dfa->trie->size, ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].child[0], ==, 2);
   g_assert_cmpint(dfa->trie->nodes[0].child[1], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[0].child[2], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].flags, ==, 0b0101);
   // This will create nodes up to level 3.
   // [0] --0--> [1] --0--> [2] --0--> [3] --0--> ()
   //  '                                '----1--> ()
   //  '----2--> ()
   rval = trie_insert(dfa, test_path[2], 3);
   g_assert_cmpint(rval, ==, 0);
   g_assert_cmpint(dfa->trie->pos, ==, 4);
   g_assert_cmpint(dfa->trie->size, ==, 4);
   g_assert_cmpint(dfa->trie->nodes[0].child[0], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].child[1], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[0].child[2], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].flags, ==, 0b0100);

   g_assert_cmpint(dfa->trie->nodes[1].child[0], ==, 2);
   g_assert_cmpint(dfa->trie->nodes[1].child[1], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[1].child[2], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[1].flags, ==, 0b0000);

   g_assert_cmpint(dfa->trie->nodes[2].child[0], ==, 3);
   g_assert_cmpint(dfa->trie->nodes[2].child[1], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[2].child[2], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[2].flags, ==, 0b0000);

   g_assert_cmpint(dfa->trie->nodes[3].child[0], ==, 2);
   g_assert_cmpint(dfa->trie->nodes[3].child[1], ==, 3);
   g_assert_cmpint(dfa->trie->nodes[3].child[2], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[3].flags, ==, 0b0011);
   // Insert center branch in the trie.
   // [0] --0--> [1] --0--> [2] --0--> [3] --0--> ()
   //  '----1--> ()                     '----1--> ()
   //  '----2--> ()
   rval = trie_insert(dfa, test_path[3], 4);
   g_assert_cmpint(rval, ==, 0);
   g_assert_cmpint(dfa->trie->pos, ==, 4);
   g_assert_cmpint(dfa->trie->size, ==, 4);
   g_assert_cmpint(dfa->trie->nodes[0].child[0], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].child[1], ==, 4);
   g_assert_cmpint(dfa->trie->nodes[0].child[2], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].flags, ==, 0b0110);
   // Insert middle branch in the trie.
   rval = trie_insert(dfa, test_path[4], 5);
   g_assert_cmpint(rval, ==, 0);
   g_assert_cmpint(dfa->trie->pos, ==, 5);
   g_assert_cmpint(dfa->trie->size, ==, 8);
   g_assert_cmpint(dfa->trie->nodes[0].child[0], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].child[1], ==, 4);
   g_assert_cmpint(dfa->trie->nodes[0].child[2], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].flags, ==, 0b0100);

   g_assert_cmpint(dfa->trie->nodes[4].child[0], ==, 5);
   g_assert_cmpint(dfa->trie->nodes[4].child[1], ==, 4);
   g_assert_cmpint(dfa->trie->nodes[4].child[2], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[4].flags, ==, 0b0011);

   // Move initial path.
   rval = trie_insert(dfa, test_path[5], 6);
   g_assert_cmpint(rval, ==, 0);
   g_assert_cmpint(dfa->trie->pos, ==, 6);
   g_assert_cmpint(dfa->trie->size, ==, 8);
   g_assert_cmpint(dfa->trie->nodes[0].child[0], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].child[1], ==, 4);
   g_assert_cmpint(dfa->trie->nodes[0].child[2], ==, 5);
   g_assert_cmpint(dfa->trie->nodes[0].flags, ==, 0b0000);

   g_assert_cmpint(dfa->trie->nodes[5].child[0], ==, 6);
   g_assert_cmpint(dfa->trie->nodes[5].child[1], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[5].child[2], ==, 0);
   g_assert_cmpint(dfa->trie->nodes[5].flags, ==, 0b0011);

   // Bad insertion.
   uint pos = dfa->trie->pos;
   rval = trie_insert(dfa, test_path[8], 9);
   g_assert_cmpint(rval, ==, -1);
   g_assert_cmpint(dfa->trie->pos, ==, pos);

   // Alloc test.
   mute_stderr();
   set_alloc_failure_rate_to(1.0);
   uint8_t new_path[10] = {1,2};
   int ret1 = trie_insert(dfa, new_path, 10);
   new_path[0] = 0;
   int ret2 = trie_insert(dfa, test_path[6], 7);
   reset_alloc();
   unmute_stderr();
   g_assert_cmpint(ret1, ==, 0);
   // Realloc fails.
   g_assert_cmpint(ret2, ==, -1);

   dfa_free(dfa);

   // Try height 0.
   trie_height = 1;
   trie_nodes = 1;
   dfa_t * lowdfa = dfa_new(trie_height,0,100,trie_nodes,0);
   g_assert(lowdfa != NULL);
   g_assert_cmpint(lowdfa->trie->height, ==, 1);

   // Test paths.
   uint8_t test_path2[3][3] = {
      {2,1,1},
      {0,0,0},
      {0,1,2}};

   // Insert path references.
   for (int i = 0; i < 3; i++) {
      vertex_t * vertex = (vertex_t *) (lowdfa->states + (i+1) * lowdfa->state_size);
      path_encode(test_path2[i], vertex->code, trie_height);
   }


   // Insert a path longer than trie height.
   rval = trie_insert(lowdfa, test_path2[1], 2);
   g_assert_cmpint(rval, ==, 0);
   g_assert_cmpint(lowdfa->trie->pos, ==, 1);
   g_assert_cmpint(lowdfa->trie->nodes[0].child[0], ==, 2);
   g_assert_cmpint(lowdfa->trie->nodes[0].child[2], ==, 1);
   g_assert_cmpint(lowdfa->trie->nodes[0].flags, ==, 0b0101);
   // This will replace the path '\0' (height is 1).
   rval = trie_insert(lowdfa, test_path2[2], 3);
   g_assert_cmpint(rval, ==, 0);
   g_assert_cmpint(lowdfa->trie->pos, ==, 1);
   g_assert_cmpint(lowdfa->trie->nodes[0].child[0], ==, 3);
   g_assert_cmpint(lowdfa->trie->nodes[0].child[2], ==, 1);
   g_assert_cmpint(lowdfa->trie->nodes[0].flags, ==, 0b0101);

   dfa_free(lowdfa);

   return;
}


void
test_trie_search
(void)
{
   int trie_nodes = 100;
   int trie_height = 10;
   dfa_t * dfa = dfa_new(trie_height,0,100,trie_nodes,0);
   g_assert(dfa != NULL);

   uint8_t test_path[7][10] = {
      {0,0,0,0,0,0,0,0,0,0},
      {0,0,0,1,0,0,0,0,0,0},
      {1,1,1,1,1,1,1,1,1,1},
      {1,0,1,0,1,0,1,0,1,0},
      {2,0,1,0,1,0,1,0,1,0},
      {2,1,1,1,1,1,1,1,1,0},
      {1,1,1,1,1,1,1,1,1,0}};

   uint8_t search_path[11][10] = {
      {0,0,0,0,0,0,0,0,0,1},
      {0,0,2,1,0,0,0,2,0,1},
      {2,0,2,0,0,0,0,0,0,0},
      {1,1,1,0,2,0,0,0,0,0},
      {2,1,2,0,1,0,0,0,2,2},
      {0,0,0,1,0,0,0,0,0,1},
      {1,1,1,0,0,2,0,0,0,1},
      {1,2,0,1,2,0,1,2,0,1},
      {1,0,1,0,1,0,1,0,1,1},
      {0,0,0,0,0,0,0,0,0,2},
      {3,0,0,1,1,2,0,0,1,2}};

   trie_t *trie = trie_new(1,10);
   g_assert(trie != NULL);

   // Insert path references.
   for (int i = 0; i < 7; i++) {
      vertex_t * vertex = (vertex_t *) (dfa->states + (i+2) * dfa->state_size);
      path_encode(test_path[i], vertex->code, trie_height);
      g_assert(trie_insert(dfa, test_path[i], i+2) == 0);
   }

   // Search paths.
   uint32_t dfastate;
   for (int i = 0; i < 7; i++) {
      g_assert_cmpint(trie_search(dfa, test_path[i], &dfastate), ==, 1);
      g_assert_cmpint(dfastate, ==, i+2);
   }

   for (int i = 0; i < 10; i++) {
      g_assert_cmpint(trie_search(dfa, search_path[i], &dfastate), ==, 0);
   }

   // Wrong path.
   g_assert_cmpint(trie_search(dfa,search_path[10], NULL), ==, -1);
   g_assert_cmpint(seeqerr, ==, 6);

   dfa_free(dfa);

   return;

}

void
test_dfa_new
(void)
{
   dfa_t * dfa = dfa_new(10, 3, 1, 1, 0);
   g_assert(dfa != NULL);
   g_assert(dfa->trie != NULL);
   g_assert_cmpint(dfa->size, ==, 2);
   g_assert_cmpint(dfa->pos, ==, 2);
   g_assert_cmpint(dfa->trie->pos, ==, 1);
   g_assert_cmpint(dfa->trie->size, ==, 1);
   uint32_t dfa_root;
   uint8_t path[10] = {2,2,2,2,1,1,1,1,1,1};
   g_assert_cmpint(trie_search(dfa, path, &dfa_root), ==, 1);
   g_assert_cmpint(dfa_root, ==, DFA_ROOT_STATE);
   dfa_free(dfa);

   dfa = dfa_new(10, 3, 0, 0, 0);
   g_assert(dfa != NULL);
   g_assert(dfa->trie != NULL);
   g_assert_cmpint(dfa->size, ==, 2);
   g_assert_cmpint(dfa->pos, ==, 2);
   g_assert_cmpint(dfa->trie->pos, ==, 1);
   g_assert_cmpint(dfa->trie->size, ==, 1);
   dfa_free(dfa);

   for (int i = 0; i < 1000; i++) {
      dfa = dfa_new(10, 3, i, i, 0);
      g_assert(dfa != NULL);
      g_assert(dfa->trie != NULL);
      g_assert_cmpint(dfa->size, ==, (i < 2 ? 2 : i));
      g_assert_cmpint(dfa->pos, ==, 2);
      g_assert_cmpint(dfa->trie->pos, ==, 1);
      g_assert_cmpint(dfa->trie->size, ==, (i < 1 ? 1 : i));
      dfa_free(dfa);
   }


   dfa = dfa_new(10, -1, 10, 10, 0);
   g_assert(dfa == NULL);

   dfa = dfa_new(0, 3, 10, 10, 0);
   g_assert(dfa == NULL);

   // Alloc test.
   mute_stderr();
   set_alloc_failure_rate_to(1.1);
   dfa = dfa_new(10, 3, 1, 1, 0);
   reset_alloc();
   unmute_stderr();
   g_assert(dfa == NULL);

   // Exhaustive alloc test
   set_alloc_failure_rate_to(0.1);
   for (int i = 0; i < 10000; i++) {
      dfa = dfa_new(10, 3, 1, 1, 0);
      if (dfa != NULL) dfa_free(dfa);
   }
   reset_alloc();
}


void
test_dfa_newvertex
(void)
{
   dfa_t * dfa = dfa_new(10, 3, 1, 1, 0);
   g_assert(dfa != NULL);
   g_assert_cmpint(dfa->size, ==, 2);
   g_assert_cmpint(dfa->pos, ==, 2);

   for (int i = 1; i < 1023; i++) {
      uint32_t new = dfa_newvertex(&dfa);
      g_assert_cmpint(dfa->pos, ==, i+2);
      vertex_t * vertex = (vertex_t *) (dfa->states + new * dfa->state_size);
      g_assert_cmpint(vertex->match, ==, DFA_COMPUTE);
      for (int j = 0; j < NBASES; j++) {
         g_assert_cmpint(vertex->next[j], ==, DFA_COMPUTE);
      }
   }
   g_assert_cmpint(dfa->size, ==, 1024);

   // Alloc test.
   mute_stderr();
   set_alloc_failure_rate_to(1.1);
   int ret = dfa_newvertex(&dfa);
   reset_alloc();
   unmute_stderr();
   g_assert_cmpint(ret, ==, -1);

}


void
test_dfa_newstate
(void)
{
   dfa_t * dfa = dfa_new(5, 2, 1, 1, 0);
   g_assert(dfa != NULL);
   g_assert(dfa->trie != NULL);
   g_assert_cmpint(dfa->size, ==, 2);
   g_assert_cmpint(dfa->pos, ==, 2);
   g_assert_cmpint(dfa->trie->size, ==, 1);
   g_assert_cmpint(dfa->trie->pos, ==, 1);

   uint8_t path[5] = {2,2,2,1,1};
   vertex_t * vertex = (vertex_t *) (dfa->states + 1 * dfa->state_size);
   g_assert_cmpint(path_compare(path, vertex->code, 5), ==, 1);

   // Insert states.
   uint8_t new_path[5] = {1,2,2,2,1};
   g_assert(dfa_newstate(&dfa, new_path, 3, 1) == 0);
   vertex = (vertex_t *) (dfa->states + 1 * dfa->state_size);
   g_assert_cmpint(vertex->next[3], ==, 2);
   g_assert_cmpint(dfa->size, ==, 4);
   g_assert_cmpint(dfa->pos, ==, 3);
   g_assert_cmpint(dfa->trie->pos, ==, 1);
   g_assert_cmpint(dfa->trie->size, ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].child[1], ==, 2);
   g_assert_cmpint(dfa->trie->nodes[0].child[2], ==, 1);
   g_assert_cmpint(dfa->trie->nodes[0].flags, ==, 0b0110);
   vertex = (vertex_t *) (dfa->states + 2 * dfa->state_size);
   g_assert_cmpint(path_compare(new_path, vertex->code, 5), ==, 1);

   // Wrong code.
   new_path[3] = 3;
   size_t pos = dfa->pos;
   g_assert(dfa_newstate(&dfa, new_path, 0, 1) == -1);
   g_assert_cmpint(dfa->pos, ==, pos);

   // Alloc error when extending dfa.
   mute_stderr();
   new_path[2] = 1; new_path[3] = 2; new_path[4] = 2;
   set_alloc_failure_rate_to(1.1);
   pos = dfa->pos;
   int ret = dfa_newstate(&dfa, new_path, 0, 1);
   g_assert_cmpint(dfa->pos, ==, pos);
   reset_alloc();
   unmute_stderr();
   g_assert_cmpint(ret, ==, -1);
}


void
test_dfa_step
(void)
{
   char   * pattern = "CATG";
   char   * text = "ATCCTCATGA";
   uint     tau = 1;
   char     exp[strlen(pattern)];
   uint     plen = parse(pattern, exp);
   g_assert_cmpint(plen, ==, 4);

   dfa_t  * dfa = dfa_new(plen, tau, 1, 1, 0);
   g_assert(dfa != NULL);

   uint32_t state = DFA_ROOT_STATE;
   // text[0] A
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[0]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 2);
   vertex_t * vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 2);
   // text[1] T
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[1]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 3);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 1);
   // text[2] C
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[2]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 4);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 2);
   // text[3] C
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[3]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 4);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 2);
   // text[4] T
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[4]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 5);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 1);
   // text[5] C
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[5]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 4);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 2);
   // text[6] A
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[6]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 6);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 1);
   // text[7] T
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[7]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 7);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 1);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 0);
   // text[8] G
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[8]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 8);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 0);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 0);
   // text[9] A
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[9]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 9);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 1);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 0);
   // recover existing step.
   g_assert(0 == dfa_step(1, translate_ignore[(int)text[0]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 2);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 2);

   dfa_free(dfa);

   // Try the same with memory limit to 50 bytes (only root node).
   // The first 2 dfa states and the trie node will fit, but nothing else.
   pattern = "AAAA";
   text = "ATTAAAT";
   tau = 1;
   plen = parse(pattern, exp);
   g_assert_cmpint(plen, ==, 4);

   dfa = dfa_new(plen, tau, 1, 1, 50);
   g_assert(dfa != NULL);

   state = DFA_ROOT_STATE;
   // text[0] A
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[0]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 0);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 2);

   // text[1] T
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[1]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 0);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 2);

   // text[2] T
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[2]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 1);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 3);

   // text[3] A
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[3]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 0);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 2);

   // text[4] A
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[4]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 0);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 2);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 1);

   // text[5] A
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[5]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 0);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 1);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 0);

   // text[6] T
   g_assert(0 == dfa_step(state, translate_ignore[(int)text[6]], plen, tau, &dfa, exp, &state));
   g_assert_cmpint(state, ==, 0);
   vertex = (vertex_t *) (dfa->states + state * dfa->state_size);
   g_assert_cmpint(get_match(vertex->match), ==, 1);
   g_assert_cmpint(get_mintomatch(vertex->match), ==, 0);

   dfa_free(dfa);
   

   // Alloc exhaustive test.
   /*
   pattern = "ATCGATCGATCGACG";
   char exp2[strlen(pattern)];
   plen = parse(pattern, exp2);
   dfa = dfa_new(plen, tau, 1, 1);

   mute_stderr();
   set_alloc_failure_rate_to(0.1);
   transition.state = 0;
   for (int i = 0; i < 1000; i++) {
      int base = lrand48() % NBASES;
      if (dfa_step(transition.state, base, plen, tau, &dfa, exp2, &transition) == -1) {
         dfa = dfa_new(plen, tau, 1, 1);
         transition.state = 0;
      }
   }
   reset_alloc();
   */

   return;
}

void
test_parse
(void)
{

   char keys[100];
   g_assert_cmpint(parse("AaAaAaAa", keys), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keys[i], ==, 1);
   }

   g_assert_cmpint(parse("CcCcCcCc", keys), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keys[i], ==, 2);
   }

   g_assert_cmpint(parse("GgGgGgGg", keys), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keys[i], ==, 4);
   }

   g_assert_cmpint(parse("TtTtTtTt", keys), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keys[i], ==, 8);
   }

   g_assert_cmpint(parse("NnNnNnNn", keys), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keys[i], ==, 31);
   }

   g_assert_cmpint(parse("Nn[]Nn[]NnN[]n", keys), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keys[i], ==, 31);
   }

   g_assert_cmpint(parse("[GATC][gatc][GaTc][gAtC]", keys), ==, 4);
   for (int i = 0 ; i < 4 ; i++) {
      g_assert_cmpint(keys[i], ==, 15);
   }

   g_assert_cmpint(parse("[GATCgatc", keys), ==, -1);
   g_assert_cmpint(parse("A]", keys), ==, -1);
   g_assert_cmpint(parse("[ATG[C]]", keys), ==, -1);
   g_assert_cmpint(parse("Z", keys), ==, -1);

   return;

}

void
test_seeqNew
(void)
{
   // Open file and check struct contents.
   seeq_t * sq = seeqNew("ACTGA", 2, 0);
   g_assert(sq != NULL);

   // seeq_t
   g_assert_cmpint(sq->hits, ==, 0);
   g_assert_cmpint(sq->stacksize, ==, INITIAL_MATCH_STACK_SIZE);
   g_assert_cmpint(sq->tau, ==, 2);
   g_assert_cmpint(sq->wlen, ==, 5);
   g_assert_cmpint(sq->keys[0], ==, 1);
   g_assert_cmpint(sq->keys[1], ==, 2);
   g_assert_cmpint(sq->keys[2], ==, 8);
   g_assert_cmpint(sq->keys[3], ==, 4);
   g_assert_cmpint(sq->keys[4], ==, 1);
   g_assert(sq->dfa  != NULL);
   g_assert(sq->rdfa != NULL);
   g_assert(sq->match != NULL);
   g_assert(sq->string == NULL);
   seeqFree(sq);
   
   // Open stdin.
   sq = seeqNew("ACG[AT]", 1, 0);
   g_assert(sq != NULL);
   g_assert_cmpint(sq->hits, ==, 0);
   g_assert_cmpint(sq->stacksize, ==, INITIAL_MATCH_STACK_SIZE);
   g_assert_cmpint(sq->tau, ==, 1);
   g_assert_cmpint(sq->wlen, ==, 4);
   g_assert_cmpint(sq->keys[0], ==, 1);
   g_assert_cmpint(sq->keys[1], ==, 2);
   g_assert_cmpint(sq->keys[2], ==, 4);
   g_assert_cmpint(sq->keys[3], ==, 9);
   g_assert(sq->dfa  != NULL);
   g_assert(sq->rdfa != NULL);
   g_assert(sq->match != NULL);
   g_assert(sq->string == NULL);
   seeqFree(sq);

   // Check tau error.
   sq = seeqNew("ACG[AT]", -1, 0);
   g_assert(sq == NULL);
   g_assert_cmpint(seeqerr, ==, 1);

   sq = seeqNew("ACG[AT]", 4, 0);
   g_assert(sq == NULL);
   g_assert_cmpint(seeqerr, ==, 9);


   // Incorrect pattern.
   sq = seeqNew("ACT[A[AG]", 1, 0);
   g_assert(sq == NULL);
   g_assert_cmpint(seeqerr, ==, 2);
   sq = seeqNew("ACT[AG]T]A", 1, 0);
   g_assert(sq == NULL);
   g_assert_cmpint(seeqerr, ==, 3);
   sq = seeqNew("ACHT[AG]", 1, 0);
   g_assert(sq == NULL);
   g_assert_cmpint(seeqerr, ==, 4);
   sq = seeqNew("ACT[AG]A[TG", 1, 0);
   g_assert(sq == NULL);
   g_assert_cmpint(seeqerr, ==, 5);
}

void
test_seeqFileMatch
(void)
{
   seeqfile_t * sqfile = seeqOpen("testdata.txt");
   g_assert(sqfile != NULL);
   seeq_t * sq = seeqNew("ATCG", 1, 0);
   g_assert(sq != NULL);
   g_assert_cmpint(seeqFileMatch(sqfile, sq, SQ_FIRST, SQ_MATCH), ==, 1);
   g_assert_cmpint(sq->hits, ==, 1);
   match_t * match = seeqMatchIter(sq);
   g_assert(match != NULL);
   g_assert(seeqMatchIter(sq) == NULL);
   g_assert_cmpint(match->start, ==, 2);
   g_assert_cmpint(match->end, ==, 5);
   g_assert_cmpint(match->dist, ==, 1);
   g_assert_cmpint(sqfile->line, ==, 1);
   g_assert_cmpstr(seeqGetString(sq), ==, "GTATGTACCACAGATGTCGATCGAC");

   g_assert_cmpint(seeqFileMatch(sqfile, sq, SQ_FIRST, SQ_MATCH), ==, 1);
   g_assert_cmpint(sq->hits, ==, 1);
   match = seeqMatchIter(sq);
   g_assert_cmpint(match->start, ==, 3);
   g_assert_cmpint(match->end, ==, 7);
   g_assert_cmpint(match->dist, ==, 1);
   g_assert_cmpint(sqfile->line, ==, 2);
   g_assert_cmpstr(seeqGetString(sq), ==, "TCTATCATCCGTACTCTGATCTCAT");
   g_assert_cmpint(seeqFileMatch(sqfile, sq, SQ_FIRST, SQ_MATCH), ==, 0);
   seeqClose(sqfile);
   seeqFree(sq);

   sqfile = seeqOpen("testdata.txt");
   g_assert(sqfile != NULL);
   sq = seeqNew("TGTC", 1, 0);
   g_assert(sq != NULL);
   g_assert_cmpint(seeqFileMatch(sqfile, sq, SQ_BEST, SQ_MATCH), ==, 1);
   g_assert_cmpint(sq->hits, ==, 1);
   match = seeqMatchIter(sq);
   g_assert_cmpint(match->start, ==, 14);
   g_assert_cmpint(match->end, ==, 18);
   g_assert_cmpint(match->dist, ==, 0);
   g_assert_cmpint(sqfile->line, ==, 1);
   g_assert_cmpstr(seeqGetString(sq), ==, "GTATGTACCACAGATGTCGATCGAC");

   g_assert_cmpint(seeqFileMatch(sqfile, sq, SQ_BEST, SQ_MATCH), ==, 1);
   g_assert_cmpint(sq->hits, ==, 1);
   match = seeqMatchIter(sq);
   g_assert_cmpint(match->start, ==, 2);
   g_assert_cmpint(match->end, ==, 6);
   g_assert_cmpint(match->dist, ==, 1);
   g_assert_cmpint(sqfile->line, ==, 2);
   g_assert_cmpstr(seeqGetString(sq), ==, "TCTATCATCCGTACTCTGATCTCAT");
   seeqClose(sqfile);
   seeqFree(sq);

   sqfile = seeqOpen("testdata.txt");
   g_assert(sqfile != NULL);
   sq = seeqNew("CACAGAT", 1, 0);
   g_assert(sq != NULL);
   g_assert_cmpint(seeqFileMatch(sqfile, sq, 0, SQ_NOMATCH), ==, 1);
   g_assert_cmpint(sq->hits, ==, 0);
   g_assert_cmpint(sqfile->line, ==, 2);
   g_assert_cmpstr(seeqGetString(sq), ==, "TCTATCATCCGTACTCTGATCTCAT");

   g_assert_cmpint(seeqFileMatch(sqfile, sq, 0, SQ_NOMATCH), ==, 1);
   g_assert_cmpint(sq->hits, ==, 0);
   g_assert_cmpint(sqfile->line, ==, 3);
   g_assert_cmpstr(seeqGetString(sq), ==, "RCACAGATCACAGATCACAGRATCAC");

   seeqClose(sqfile);
   
   sqfile = seeqOpen("testdata.txt");
   g_assert(sqfile != NULL);
   g_assert(sq != NULL);
   g_assert_cmpint(seeqFileMatch(sqfile, sq, SQ_BEST, SQ_ANY), ==, 1);
   g_assert_cmpint(sq->hits, ==, 1);
   match = seeqMatchIter(sq);
   g_assert_cmpint(match->start, ==, 8);
   g_assert_cmpint(match->end, ==, 15);
   g_assert_cmpint(match->dist, ==, 0);
   g_assert_cmpint(sqfile->line, ==, 1);
   g_assert_cmpstr(seeqGetString(sq), ==, "GTATGTACCACAGATGTCGATCGAC");

   g_assert_cmpint(seeqFileMatch(sqfile, sq, SQ_BEST, SQ_ANY), ==, 1);
   g_assert_cmpint(sq->hits, ==, 0);
   g_assert_cmpstr(seeqGetString(sq), ==, "TCTATCATCCGTACTCTGATCTCAT");

   g_assert_cmpint(seeqFileMatch(sqfile, sq, 0, SQ_MATCH), ==, 0);
   seeqClose(sqfile);
   seeqFree(sq);

   sqfile = seeqOpen("testdata.txt");
   g_assert(sqfile != NULL);
   sq = seeqNew("ATC", 0, 0);
   g_assert(sq != NULL);
   g_assert_cmpint(seeqFileMatch(sqfile, sq, 0, SQ_COUNTLINES), ==, 2);
   seeqClose(sqfile);

   sqfile = seeqOpen("testdata.txt");
   g_assert(sqfile != NULL);
   g_assert(sq != NULL);
   g_assert_cmpint(seeqFileMatch(sqfile, sq, 0, SQ_COUNTMATCH), ==, 4);
   seeqClose(sqfile);

   // Force error
   sqfile = seeqOpen(NULL);
   g_assert(sqfile != NULL);
   sqfile->fdi = NULL;
   g_assert_cmpint(seeqFileMatch(sqfile, sq, 0, 0), ==, -1);
   g_assert_cmpint(seeqerr, ==, 10);
   seeqClose(sqfile);

   // String first match.
   sq = seeqNew("GATC", 1, 0);
   g_assert_cmpint(seeqStringMatch("TGACTGATGACGTAGTCTACGATCGATCAGTCA", sq, SQ_FIRST), == , 1);
   g_assert_cmpint(sq->hits, ==, 1);
   match = seeqMatchIter(sq);
   g_assert_cmpint(match->start, ==, 1);
   g_assert_cmpint(match->end, ==, 4);
   g_assert_cmpint(match->dist, ==, 1);

   // String best match.
   g_assert_cmpint(seeqStringMatch("TGACTGATGACGTAGTCTACGATCGATCAGTCA", sq, SQ_BEST), == , 1);
   g_assert_cmpint(sq->hits, ==, 1);
   match = seeqMatchIter(sq);
   g_assert_cmpint(match->start, ==, 20);
   g_assert_cmpint(match->end, ==, 24);
   g_assert_cmpint(match->dist, ==, 0);

   // String all matches.
   g_assert_cmpint(seeqStringMatch("TGACTGATGACGTAGTCTACGATCGATCAGTCA", sq, SQ_ALL), == , 7);
   g_assert_cmpint(sq->hits, ==, 7);
   g_assert_cmpint(sq->match[6].start, ==, 1);
   g_assert_cmpint(sq->match[6].end, ==, 4);
   g_assert_cmpint(sq->match[6].dist, ==, 1);

   g_assert_cmpint(sq->match[5].start, ==, 5);
   g_assert_cmpint(sq->match[5].end, ==, 9);
   g_assert_cmpint(sq->match[5].dist, ==, 1);

   g_assert_cmpint(sq->match[4].start, ==, 8);
   g_assert_cmpint(sq->match[4].end, ==, 11);
   g_assert_cmpint(sq->match[4].dist, ==, 1);

   g_assert_cmpint(sq->match[3].start, ==, 14);
   g_assert_cmpint(sq->match[3].end, ==, 17);
   g_assert_cmpint(sq->match[3].dist, ==, 1);

   g_assert_cmpint(sq->match[2].start, ==, 20);
   g_assert_cmpint(sq->match[2].end, ==, 24);
   g_assert_cmpint(sq->match[2].dist, ==, 0);

   g_assert_cmpint(sq->match[1].start, ==, 24);
   g_assert_cmpint(sq->match[1].end, ==, 28);
   g_assert_cmpint(sq->match[1].dist, ==, 0);

   g_assert_cmpint(sq->match[0].start, ==, 29);
   g_assert_cmpint(sq->match[0].end, ==, 32);
   g_assert_cmpint(sq->match[0].dist, ==, 1);

   seeqFree(sq);

   // Overlapping matches.
   sq = seeqNew("GAAG", 0, 0);
   g_assert_cmpint(seeqStringMatch("GAAGAAG", sq, SQ_ALL), == , 2);
   g_assert_cmpint(sq->hits, ==, 2);
   g_assert_cmpint(sq->match[0].start, ==, 3);
   g_assert_cmpint(sq->match[0].end, ==, 7);
   g_assert_cmpint(sq->match[0].dist, ==, 0);
   g_assert_cmpint(sq->match[1].start, ==, 0);
   g_assert_cmpint(sq->match[1].end, ==, 4);
   g_assert_cmpint(sq->match[1].dist, ==, 0);

   seeqFree(sq);

   // Overlapping matches.
   sq = seeqNew("GAAG", 1, 0);
   g_assert_cmpint(seeqStringMatch("GAAGAAG", sq, SQ_ALL), == , 2);
   g_assert_cmpint(sq->hits, ==, 2);
   g_assert_cmpint(sq->match[1].start, ==, 0);
   g_assert_cmpint(sq->match[1].end, ==, 4);
   g_assert_cmpint(sq->match[1].dist, ==, 0);

   g_assert_cmpint(sq->match[0].start, ==, 3);
   g_assert_cmpint(sq->match[0].end, ==, 7);
   g_assert_cmpint(sq->match[0].dist, ==, 0);

   seeqFree(sq);

   // Overlapping matches.
   sq = seeqNew("GAAG", 1, 0);
   g_assert_cmpint(seeqStringMatch("GAAGACG", sq, SQ_ALL), == , 2);
   g_assert_cmpint(sq->hits, ==, 2);
   g_assert_cmpint(sq->match[0].start, ==, 3);
   g_assert_cmpint(sq->match[0].end, ==, 7);
   g_assert_cmpint(sq->match[0].dist, ==, 1);

   g_assert_cmpint(sq->match[1].start, ==, 0);
   g_assert_cmpint(sq->match[1].end, ==, 4);
   g_assert_cmpint(sq->match[1].dist, ==, 0);

   seeqFree(sq);
}

void
test_seeqClose
(void)
{
   seeqfile_t * sq = seeqOpen("testdata.txt");
   g_assert_cmpint(seeqClose(sq), ==, 0);
   sq = seeqOpen(NULL);
   g_assert_cmpint(seeqClose(sq), ==, 0);
}

void
test_seeq
(void)
{

   /* All tests from this section are based on an input file called
      testdata.txt placed in the test folder with the following content:

      GTATGTACCACAGATGTCGATCGAC
      TCTATCATCCGTACTCTGATCTCAT
      LCACAGATCACAGATCACAGATCAC
   */
   signal(SIGSEGV, SIGSEGV_handler);

   struct seeqarg_t args = {
      .showdist  = 0,
      .showpos   = 0,
      .showline  = 0,
      .printline = 1,
      .matchonly = 0,
      .count     = 0,
      .compact   = 0,
      .dist      = 0,
      .verbose   = 0,
      .endline   = 0,
      .prefix    = 0,
      .invert    = 0,
      .best      = 0,
      .non_dna   = 0,
      .all       = 0,
      .memory    = 0
   };

   // Test 1: tau=0, default output.
   char * pattern = "CACAGAT";
   char * input   = "testdata.txt";
   char * answer  = "GTATGTACCACAGATGTCGATCGAC\n";

   // Redirect outputs.
   redirect_stdout_to(OUTPUT_BUFFER);
   redirect_stderr_to(ERROR_BUFFER);
   int offset = 0;

   tty_value = 1;
   args.verbose = 1;
   seeq(pattern, input, args);
   offset = strlen(OUTPUT_BUFFER);
   args.verbose = 0;

   tty_value = 0;
   seeq(pattern, input, args);
   g_assert_cmpint(strcmp(OUTPUT_BUFFER+offset, answer), ==, 0);
   offset = strlen(OUTPUT_BUFFER);

   // Test 2: tau=0, show line, distance, position.
   args.showdist = args.showpos = args.showline = 1;
   answer = "1 8-14 0 GTATGTACCACAGATGTCGATCGAC\n";
   seeq(pattern, input, args);
   g_assert_cmpint(strcmp(OUTPUT_BUFFER+offset, answer), ==, 0);
   offset = strlen(OUTPUT_BUFFER);

   // Test 3: tau=3, compact output.
   args.showdist = args.showpos = args.showline = 0;
   args.compact = 1;
   args.dist = 3;
   answer = "1:8-14:0\n2:8-11:3\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 4: tau=0, count.
   args.dist = args.compact = 0;
   args.count = 1;
   answer = "1\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 5: tau=0, inverse.
   args.count = 0;
   args.invert = 1;
   answer = "TCTATCATCCGTACTCTGATCTCAT\nRCACAGATCACAGATCACAGRATCAC\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 6: tau=0, inverse+lines.
   args.showline = 1;
   answer = "2 TCTATCATCCGTACTCTGATCTCAT\n3 RCACAGATCACAGATCACAGRATCAC\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);
   
   // Test 7: tau=3, match only.
   args.invert = args.showline = 0;
   args.matchonly = 1;
   args.dist = 3;
   answer = "CACAGAT\nCCGT\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);
  
   // Test 7.1: skip off.
   args.non_dna = 1;
   answer = "CACAGAT\nCCGT\nCACAGAT\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 7.2 (best on/off).
   args.non_dna = 0;
   args.best = args.dist = 1;
   answer = "CTCAT\n";
   seeq("CTCAT", input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);
   
   args.best = 0;
   answer = "CTAT\n";
   seeq("CTCAT", input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 8: tau=3, prefix.
   args.dist = 3;
   args.matchonly = args.printline = 0;
   args.prefix = 1;
   answer = "GTATGTAC\nTCTATCAT\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 9: tau=3, suffix (endline).
   args.prefix = 0;
   args.endline = 1;
   answer = "GTCGATCGAC\nACTCTGATCTCAT\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 11: tau=0, non_dna ignore.
   args.dist = 0;
   args.endline = 0;
   args.showline = 1;
   args.non_dna = 2;
   args.matchonly = 1;
   answer = "1 CACAGAT\n3 CACAGAT\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 10: tau=0, non_dna convert to N.
   args.all = 1;
   args.non_dna = 1;
   answer = "1 CACAGAT\n3 CACAGAT\n3 CACAGAT\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 12: tau=0, non_dna ignore, all.
   args.non_dna = 2;
   answer = "1 CACAGAT\n3 CACAGAT\n3 CACAGAT\n3 CACAGRAT\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 10: incorrect pattern.
   g_assert(seeq("CACAG[AT", input, args) == EXIT_FAILURE);
   g_assert_cmpint(seeqerr, ==, 5);

   // Test 11: tau > pattern length.
   args.dist = 7;
   g_assert(seeq(pattern, input, args) == EXIT_FAILURE);
   g_assert_cmpint(seeqerr, ==, 9);

   // Test 12: file does not exist.
   args.dist = 0;
   g_assert(seeq(pattern, "invented.txt", args) == EXIT_FAILURE);
   g_assert_cmpint(seeqerr, ==, 0);

   // Test 13: invalid distance.
   args.dist = -1;
   g_assert(seeq(pattern, input, args) == EXIT_FAILURE);
   g_assert_cmpint(seeqerr, ==, 1);

   // ALLOC FAILURE TESTS.
   mute_stderr();
   mute_stdout();
   args.dist = 0;
   set_alloc_failure_rate_to(1.0);
   int ret = seeq(pattern, input, args);
   reset_alloc();
   g_assert_cmpint(ret, ==, EXIT_FAILURE);


   // Exhaustive alloc test.
   set_alloc_failure_rate_to(0.05);
   for (int i = 0; i < 10000; i++) seeq(pattern, input, args);
   reset_alloc();

   unredirect_sderr();

   return;
}


int
main(
   int argc,
   char **argv
)
{
   // Save the stderr file descriptor upon start.
   BACKUP_FILE_DESCRIPTOR = dup(STDERR_FILENO);
   BACKUP_STDOUT = dup(STDOUT_FILENO);

   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/libseeq/core/trie_new", test_trie_new);
   g_test_add_func("/libseeq/core/trie_insert", test_trie_insert);
   g_test_add_func("/libseeq/core/trie_search", test_trie_search);
   g_test_add_func("/libseeq/core/dfa_new", test_dfa_new);
   g_test_add_func("/libseeq/core/dfa_newvertex", test_dfa_newvertex);
   g_test_add_func("/libseeq/core/dfa_newstate", test_dfa_newstate);
   g_test_add_func("/libseeq/core/dfa_step", test_dfa_step);
   g_test_add_func("/libseeq/core/parse", test_parse);
   g_test_add_func("/libseeq/lib/seeqNew", test_seeqNew);
   g_test_add_func("/libseeq/lib/seeqFileMatch", test_seeqFileMatch);
   g_test_add_func("/libseeq/lib/seeqClose", test_seeqClose);
   g_test_add_func("/seeq", test_seeq);
   return g_test_run();
}
