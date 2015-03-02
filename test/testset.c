#include <glib.h>
#include <stdio.h>
#include <string.h>
#include "faultymalloc.h"
#include "libseeq.h"

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

typedef struct {
} fixture;


// --  ERROR MESSAGE HANDLING FUNCTIONS  -- //

char ERROR_BUFFER[1024];
char OUTPUT_BUFFER[1024];
int BACKUP_FILE_DESCRIPTOR;
int BACKUP_STDOUT;

void
mute_stderr
(void)
{
   fflush(stderr);
   int fd = open("/dev/null", O_WRONLY);
   dup2(fd, STDERR_FILENO);
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


// --  SET UP AND TEAR DOWN TEST CASES  -- //
/*
void
setup(
   fixture *f,
   gconstpointer test_data
)
// SYNOPSIS:                                                             
//   Construct a very simple trie for testing purposes.                  
{
   return;
}

void
teardown(
   fixture *f,
   gconstpointer test_data
)
{
   return;
}
*/


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
      g_assert_cmpint(trie->nodes[0].parent, ==, 0);
      for (int k = 0; k < TRIE_CHILDREN; k++)
         g_assert_cmpint(trie->nodes[0].child[k], ==, 0);
      free(trie);
   }
   }

   for (int i = 0 ; i > -100 ; i--) {
   for (int j = 0 ; j > -100 ; j--) {
      trie_t *trie = trie_new(i*100, j*100);
      g_assert(trie != NULL);
      g_assert_cmpint(trie->pos, ==, 1);
      g_assert_cmpint(trie->size, ==, 1);
      g_assert_cmpint(trie->height, ==, 0);
      g_assert_cmpint(trie->nodes[0].parent, ==, 0);
      for (int k = 0; k < TRIE_CHILDREN; k++)
      g_assert_cmpint(trie->nodes[0].child[k], ==, 0);
      free(trie);
   }
   }

   // Alloc test.
   mute_stderr();
   set_alloc_failure_rate_to(1.1);
   trie_t * ret = trie_new(1, 10);
   reset_alloc();
   g_assert(ret == NULL);

   return;

}


void
test_trie_insert
(void)
{

   trie_t *trie = trie_new(1,10);
   g_assert(trie != NULL);
   uint id;

   // Insert leftmost branch in the trie.
   id = trie_insert(&trie, "\0\0\0\0\0\0\0\0\0\0", 4897892, 1205);
   g_assert_cmpint(id, ==, 10);
   g_assert_cmpint(trie->pos, ==, 11);
   g_assert_cmpint(trie->size, ==, 16);
   for (int i = 0 ; i < 10 ; i++) {
      node_t node = trie->nodes[i];
      g_assert_cmpint(node.parent, ==, (i-1 < 0 ? 0 : i-1));
      g_assert_cmpint(node.child[0], ==, i+1);
      for (int k = 1; k < TRIE_CHILDREN; k++) {
         g_assert_cmpint(node.child[k], ==, 0);
      }
   }
   node_t node = trie->nodes[10];
   g_assert_cmpint(node.child[0], ==, 4897892);
   g_assert_cmpint(node.child[1], ==, 1205);

   // Insert center branch in the trie.
   id = trie_insert(&trie, "\1\1\1\1\1\1\1\1\1\1", 22923985, 834139423);
   g_assert_cmpint(id, ==, 20);
   g_assert_cmpint(trie->pos, ==, 21);
   g_assert_cmpint(trie->size, ==, 32);
   for (int i = 1 ; i < 10 ; i++) {
      node_t node = trie->nodes[i];
      g_assert_cmpint(node.parent, ==, i-1);
      g_assert_cmpint(node.child[0], ==, i+1);
      for (int k = 1; k < TRIE_CHILDREN; k++) {
         g_assert_cmpint(node.child[k], ==, 0);
      }
   }
   for (int i = 11 ; i < 20 ; i++) {
      node_t node = trie->nodes[i];
      g_assert_cmpint(node.parent, ==, (i == 11 ? 0 : i-1));
      g_assert_cmpint(node.child[1], ==, i+1);
      for (int k = 0; k < TRIE_CHILDREN; k++) {
         if (k != 1) g_assert_cmpint(node.child[k], ==, 0);
      }
   }
   node = trie->nodes[0];
   g_assert_cmpint(node.child[0], ==, 1);
   g_assert_cmpint(node.child[1], ==, 11);
   node = trie->nodes[10];
   g_assert_cmpint(node.child[0], ==, 4897892);
   g_assert_cmpint(node.child[1], ==, 1205);
   node = trie->nodes[20];
   g_assert_cmpint(node.child[0], ==, 22923985);
   g_assert_cmpint(node.child[1], ==, 834139423);

   // Insert middle branch in the trie.
   id = trie_insert(&trie, "\1\0\1\0\1\0\1\0\1\0", 9309871, 394823344);
   g_assert_cmpint(id, ==, 29);
   g_assert_cmpint(trie->pos, ==, 30);
   g_assert_cmpint(trie->size, ==, 32);
   for (int i = 1 ; i < 10 ; i++) {
      node_t node = trie->nodes[i];
      g_assert_cmpint(node.parent, ==, i-1);
      g_assert_cmpint(node.child[0], ==, i+1);
      for (int k = 1; k < TRIE_CHILDREN; k++) {
         g_assert_cmpint(node.child[k], ==, 0);
      }
   }
   for (int i = 12 ; i < 20 ; i++) {
      node_t node = trie->nodes[i];
      g_assert_cmpint(node.parent, ==, i-1);
      g_assert_cmpint(node.child[1], ==, i+1);
      for (int k = 0; k < TRIE_CHILDREN; k++) {
         if (k != 1) g_assert_cmpint(node.child[k], ==, 0);
      }
   }

   for (int i = 21 ; i < 29 ; i++) {
      node_t node = trie->nodes[i];
      for (int k = 0; k < TRIE_CHILDREN; k++) {
         if (k != i%2) g_assert_cmpint(node.child[k], ==, 0);
         else g_assert_cmpint(node.child[k], ==, i+1);
      }
      g_assert_cmpint(node.parent, ==, (i==21 ? 11 : i-1));
   }

   node = trie->nodes[0];
   g_assert_cmpint(node.child[0], ==, 1);
   g_assert_cmpint(node.child[1], ==, 11);
   node = trie->nodes[10];
   g_assert_cmpint(node.child[0], ==, 4897892);
   g_assert_cmpint(node.child[1], ==, 1205);
   node = trie->nodes[11];
   g_assert_cmpint(node.child[0], ==, 21);
   g_assert_cmpint(node.child[1], ==, 12);
   node = trie->nodes[20];
   g_assert_cmpint(node.child[0], ==, 22923985);
   g_assert_cmpint(node.child[1], ==, 834139423);
   node = trie->nodes[29];
   g_assert_cmpint(node.child[0], ==, 9309871);
   g_assert_cmpint(node.child[1], ==, 394823344);

   // Bad insertion.
   uint pos = trie->pos;
   id = trie_insert(&trie, "\1\0\0\0\0\1\2\0\3\0", 3948234, 845722);
   g_assert_cmpint(id, ==, (uint)-1);
   g_assert_cmpint(trie->pos, ==, pos);

   free(trie);

   // Smallest tree (leaf)
   trie_t * lowtrie = trie_new(1,0);
   g_assert(lowtrie != NULL);

   // Insert a path longer than trie height.
   id = trie_insert(&lowtrie, "\0\0\0", 123, 456);
   g_assert_cmpint(id, ==, 0);
   g_assert_cmpint(lowtrie->pos, ==, 1);
   g_assert_cmpint(lowtrie->nodes[0].child[0], ==, 123);
   g_assert_cmpint(lowtrie->nodes[0].child[1], ==, 456);

   free(lowtrie);

   // Small tree.
   lowtrie = trie_new(1,1);
   g_assert(lowtrie != NULL);

   // Insert a path longer than trie height.
   id = trie_insert(&lowtrie, "\0\0\0", 123, 456);
   g_assert_cmpint(id, ==, 1);
   g_assert_cmpint(lowtrie->pos, ==, 2);
   g_assert_cmpint(lowtrie->nodes[0].child[0], ==, 1);
   g_assert_cmpint(lowtrie->nodes[1].parent, ==, 0);
   g_assert_cmpint(lowtrie->nodes[1].child[0], ==, 123);
   g_assert_cmpint(lowtrie->nodes[1].child[1], ==, 456);

   id = trie_insert(&lowtrie, "\1\0\0", 789, 100);
   g_assert_cmpint(id, ==, 2);
   g_assert_cmpint(lowtrie->pos, ==, 3);
   g_assert_cmpint(lowtrie->nodes[0].child[0], ==, 1);
   g_assert_cmpint(lowtrie->nodes[0].child[1], ==, 2);
   g_assert_cmpint(lowtrie->nodes[1].parent, ==, 0);
   g_assert_cmpint(lowtrie->nodes[2].parent, ==, 0);
   g_assert_cmpint(lowtrie->nodes[1].child[0], ==, 123);
   g_assert_cmpint(lowtrie->nodes[1].child[1], ==, 456);
   g_assert_cmpint(lowtrie->nodes[2].child[0], ==, 789);
   g_assert_cmpint(lowtrie->nodes[2].child[1], ==, 100);

   free(lowtrie);

   // Alloc test.
   mute_stderr();
   lowtrie = trie_new(1,2);
   g_assert(lowtrie != NULL);
   id = trie_insert(&lowtrie, "\0\0", 20, 20);
   g_assert_cmpint(id, ==, 2);

   set_alloc_failure_rate_to(1.0);
   int ret1 = trie_insert(&lowtrie, "\0\2", 2, 2);
   int ret2 = trie_insert(&lowtrie, "\1\2", 2, 2);
   reset_alloc();

   g_assert_cmpint(ret1, ==, 3);
   g_assert_cmpint(ret2, ==, -1);

   free(lowtrie);


   return;

}


void
test_trie_search_getrow
(void)
{

   trie_t *trie = trie_new(1,10);
   g_assert(trie != NULL);

   char path[10] = {0};
   // Insert random paths and search them.
   srand48(123);
   for (int i = 0 ; i < 10000 ; i++) {
      for (int j = 0 ; j < 10 ; j++) {
         path[j] = (lrand48() % TRIE_CHILDREN);
      }
      uint value = (uint) lrand48();
      uint dfastate = (uint) lrand48();
      uint retval, dfaval;
      uint id = trie_insert(&trie, path, value, dfastate);
      uint * row = trie_getrow(trie, id);

      g_assert(trie_search(trie, path, &retval, &dfaval));
      g_assert_cmpint(value, ==, retval);
      g_assert_cmpint(dfastate, ==, dfaval);
      g_assert_cmpint(row[trie->height], ==, value);
      for (int i = trie->height - 1; i >= 0; i--) {
         value += (path[i] == 0) - (path[i] == 2);
         g_assert_cmpint(row[i], ==, value);
      }
      free(row);
   }

   // Getrow starting from a wrong leaf.
   g_assert(trie_getrow(trie, 15) == NULL);
   g_assert(trie_getrow(trie, 4) == NULL);
   

   free(trie);

   return;

}

void
test_trie_reset
(void)
{

   trie_t *trie = trie_new(1,10);
   g_assert(trie != NULL);

   // Insert leftmost branch in the trie.
   trie_insert(&trie, "\0\0\0\0\0\0\0\0\0\0", 36636, 192438);
   g_assert_cmpint(trie->pos, ==, 11);
   g_assert_cmpint(trie->size, ==, 16);
   for (int i = 0 ; i < 10 ; i++) {
      node_t node = trie->nodes[i];
      g_assert_cmpint(node.child[0], ==, i+1);
   }
   node_t node = trie->nodes[0];
   g_assert_cmpint(node.child[0], ==, 1);
   g_assert_cmpint(node.child[1], ==, 0);
   node = trie->nodes[10];
   g_assert_cmpint(node.child[0], ==, 36636);
   g_assert_cmpint(node.child[1], ==, 192438);

   trie_reset(trie);
   g_assert(trie != NULL);
   g_assert_cmpint(trie->pos, ==, 0);
   g_assert_cmpint(trie->size, ==, 16);
   node = trie->nodes[0];
   for (int k = 0; k < TRIE_CHILDREN; k++) {
      g_assert_cmpint(node.child[k], ==, 0);
   }

   free(trie);

   return;

}

void
test_dfa_new
(void)
{
   dfa_t * dfa = dfa_new(10, 3, 1, 1);
   g_assert(dfa != NULL);
   g_assert(dfa->trie != NULL);
   g_assert_cmpint(dfa->size, ==, 1);
   g_assert_cmpint(dfa->pos, ==, 1);
   g_assert_cmpint(dfa->trie->size, ==, 16);
   uint * nwrow = trie_getrow(dfa->trie, dfa->states[0].node_id);
   for (int i = 0; i <= 10; i++) {
      g_assert_cmpint(nwrow[i], ==, (i < 4 ? i : 4));
   }

   free(dfa);

   dfa = dfa_new(10, 3, -100, -100);
   g_assert(dfa != NULL);
   g_assert(dfa->trie != NULL);
   g_assert_cmpint(dfa->size, ==, 1);
   g_assert_cmpint(dfa->pos, ==, 1);
   g_assert_cmpint(dfa->trie->size, ==, 16);

   free(dfa);

   for (int i = -100; i < 1000; i++) {
      dfa = dfa_new(10, 3, i, i);
      g_assert(dfa != NULL);
      g_assert(dfa->trie != NULL);
      g_assert_cmpint(dfa->size, ==, (i < 1 ? 1 : i));
      g_assert_cmpint(dfa->pos, ==, 1);
      free(dfa);
   }


   dfa = dfa_new(10, -1, 10, 10);
   g_assert(dfa == NULL);

   dfa = dfa_new(-1, 3, 10, 10);
   g_assert(dfa == NULL);

   // Alloc test.
   mute_stderr();
   set_alloc_failure_rate_to(1.1);
   dfa = dfa_new(10, 3, 1, 1);
   reset_alloc();
   g_assert(dfa == NULL);

   // Exhaustive alloc test
   set_alloc_failure_rate_to(0.1);
   for (int i = 0; i < 10000; i++) {
      dfa = dfa_new(10, 3, 1, 1);
      if (dfa != NULL) dfa_free(dfa);
   }
   reset_alloc();

}


void
test_dfa_newvertex
(void)
{
   dfa_t * dfa = dfa_new(10, 3, 1, 1);
   g_assert(dfa != NULL);
   g_assert_cmpint(dfa->size, ==, 1);
   g_assert_cmpint(dfa->pos, ==, 1);

   for (int i = 1; i < 1024; i++) {
      dfa_newvertex(&dfa, i);
      g_assert_cmpint(dfa->pos, ==, i+1);
      g_assert_cmpint(dfa->states[i].node_id, ==, i);
      for (int j = 0; j < NBASES; j++) {
         g_assert_cmpint(dfa->states[i].next[j].state, ==, 0);
         g_assert_cmpint(dfa->states[i].next[j].match, ==, DFA_COMPUTE);
      }
   }
   g_assert_cmpint(dfa->size, ==, 1024);

   // Alloc test.
   mute_stderr();
   set_alloc_failure_rate_to(1.1);
   int ret = dfa_newvertex(&dfa, 1024);
   reset_alloc();
   g_assert_cmpint(ret, ==, -1);

}


void
test_dfa_newstate
(void)
{
   dfa_t * dfa = dfa_new(5, 2, 1, 1);
   g_assert(dfa != NULL);
   g_assert(dfa->trie != NULL);
   g_assert_cmpint(dfa->size, ==, 1);
   g_assert_cmpint(dfa->pos, ==, 1);
   g_assert_cmpint(dfa->trie->size, ==, 8);
   uint * nwrow = trie_getrow(dfa->trie, dfa->states[0].node_id);
   for (int i = 0; i <= 5; i++) {
      g_assert_cmpint(nwrow[i], ==, (i < 3 ? i : 3));
   }

   // Insert states.
   char * code = "\1\2\2\2\1";
   g_assert(dfa_newstate(&dfa, code, 3, 0, 0) == 0);
   g_assert_cmpint(dfa->states[0].next[0].state, ==, 1);
   g_assert_cmpint(dfa->trie->size, ==, 16);
   g_assert_cmpint(dfa->trie->nodes[dfa->states[1].node_id].child[0], ==, 3);
   g_assert_cmpint(dfa->trie->nodes[dfa->states[1].node_id].child[1], ==, 1);

   // Wrong code.
   code = "\1\2\2\3\1";
   g_assert(dfa_newstate(&dfa, code, 3, 0, 0) == -1);

   // Alloc error when extending dfa.
   mute_stderr();
   code = "\1\2\1\2\2"; 
   set_alloc_failure_rate_to(1.1);
   int ret = dfa_newstate(&dfa, code, 3, 0, 0);
   reset_alloc();
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

   dfa_t  * dfa = dfa_new(plen, tau, 1, 1);
   g_assert(dfa != NULL);

   edge_t transition;   
   // text[0]
   g_assert(0 == dfa_step(0, translate[(int)text[0]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 1);
   g_assert_cmpint(transition.match, ==, 2);
   // text[1]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[1]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 2);
   g_assert_cmpint(transition.match, ==, 2);
   // text[2]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[2]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 3);
   g_assert_cmpint(transition.match, ==, 2);
   // text[3]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[3]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 3);
   g_assert_cmpint(transition.match, ==, 2);
   // text[4]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[4]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 4);
   g_assert_cmpint(transition.match, ==, 2);
   // text[5]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[5]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 3);
   g_assert_cmpint(transition.match, ==, 2);
   // text[6]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[6]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 5);
   g_assert_cmpint(transition.match, ==, 2);
   // text[7]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[7]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 6);
   g_assert_cmpint(transition.match, ==, 1);
   // text[8]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[8]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 7);
   g_assert_cmpint(transition.match, ==, 0);
   // text[9]
   g_assert(0 == dfa_step(transition.state, translate[(int)text[9]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 8);
   g_assert_cmpint(transition.match, ==, 1);
   // recover existing step.
   g_assert(0 == dfa_step(0, translate[(int)text[0]], plen, tau, &dfa, exp, &transition));
   g_assert_cmpint(transition.state, ==, 1);
   g_assert_cmpint(transition.match, ==, 2);
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
      .invert    = 0 };

   // Test 1: tau=0, default output.
   char * pattern = "CACAGAT";
   char * input   = "testdata.txt";
   char * answer  = "GTATGTACCACAGATGTCGATCGAC\n";

   // Redirect outputs.
   redirect_stdout_to(OUTPUT_BUFFER);
   redirect_stderr_to(ERROR_BUFFER);
   int offset = 0;
   int eoffset = 0;
   
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
   answer = "TCTATCATCCGTACTCTGATCTCAT\n";
   seeq(pattern, input, args);
   g_assert_cmpstr(OUTPUT_BUFFER+offset, ==, answer);
   offset = strlen(OUTPUT_BUFFER);

   // Test 6: tau=0, inverse+lines.
   args.showline = 1;
   answer = "2 TCTATCATCCGTACTCTGATCTCAT\n";
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

   // Test 8: tau=3, prefix.
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

   // Test 10: incorrect pattern.
   g_assert(seeq("CACAG[AT", input, args) == EXIT_FAILURE);
   g_assert_cmpstr(ERROR_BUFFER + eoffset, ==, "error: invalid pattern expression.\n");
   eoffset = strlen(ERROR_BUFFER);

   // Test 11: tau > pattern length.
   args.dist = 7;
   g_assert(seeq(pattern, input, args) == EXIT_FAILURE);
   g_assert_cmpstr(ERROR_BUFFER + eoffset, ==, "error: expression must be longer than the maximum distance.\n");
   eoffset = strlen(ERROR_BUFFER);

   // Test 12: file does not exist.
   args.dist = 0;
   g_assert(seeq(pattern, "invented.txt", args) == EXIT_FAILURE);
   g_assert_cmpint(strncmp(ERROR_BUFFER + eoffset, "error: could not open file:", 27), ==, 0);
   eoffset = strlen(ERROR_BUFFER);

   // Test 13: invalid distance.
   args.dist = -1;
   g_assert(seeq(pattern, input, args) == EXIT_FAILURE);
   g_assert_cmpstr(ERROR_BUFFER + eoffset, ==, "error: invalid distance.\n");
   eoffset = strlen(ERROR_BUFFER);

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
   g_test_add_func("/trie_new", test_trie_new);
   g_test_add_func("/trie_insert", test_trie_insert);
   g_test_add_func("/trie_search_getrow", test_trie_search_getrow);
   g_test_add_func("/trie_reset", test_trie_reset);
   g_test_add_func("/dfa_new", test_dfa_new);
   g_test_add_func("/dfa_newvertex", test_dfa_newvertex);
   g_test_add_func("/dfa_newstate", test_dfa_newstate);
   g_test_add_func("/dfa_step", test_dfa_step);
   g_test_add_func("/parse", test_parse);
   g_test_add_func("/seeq", test_seeq);
   return g_test_run();
}
