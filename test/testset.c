#include <glib.h>
#include <stdio.h>
#include <string.h>
#include "faultymalloc.h"
#include "seeq.h"


typedef struct {
} fixture;


// --  ERROR MESSAGE HANDLING FUNCTIONS  -- //

char ERROR_BUFFER[1024];
int BACKUP_FILE_DESCRIPTOR;

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

   free(trie);

   // Small tree
   trie_t *lowtrie = trie_new(1,1);
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

   // Smallest tree (leaf)
   lowtrie = trie_new(1,0);
   g_assert(lowtrie != NULL);

   // Insert a path longer than trie height.
   id = trie_insert(&lowtrie, "\0\0\0", 123, 456);
   g_assert_cmpint(id, ==, 0);
   g_assert_cmpint(lowtrie->pos, ==, 1);
   g_assert_cmpint(lowtrie->nodes[0].child[0], ==, 123);
   g_assert_cmpint(lowtrie->nodes[0].child[1], ==, 456);

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
   }

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
   dfa_t * dfa = dfa_new(1);
   g_assert(dfa != NULL);
   g_assert_cmpint(dfa->size, ==, 1);
   g_assert_cmpint(dfa->pos, ==, 0);
   free(dfa);

   dfa = dfa_new(-100);
   g_assert(dfa != NULL);
   g_assert_cmpint(dfa->size, ==, 1);
   g_assert_cmpint(dfa->pos, ==, 0);
   free(dfa);

   for (int i = -100; i < 1000; i++) {
      dfa = dfa_new(i);
      g_assert(dfa != NULL);
      g_assert_cmpint(dfa->size, ==, (i < 1 ? 1 : i));
      g_assert_cmpint(dfa->pos, ==, 0);
      free(dfa);
   }
}


void
test_dfa_newvertex
(void)
{
   dfa_t * dfa = dfa_new(1);
   g_assert(dfa != NULL);
   g_assert_cmpint(dfa->size, ==, 1);
   g_assert_cmpint(dfa->pos, ==, 0);

   for (int i = 0; i < 1000; i++) {
      dfa_newvertex(&dfa, i);
      g_assert_cmpint(dfa->pos, ==, i+1);
      g_assert_cmpint(dfa->states[i].node_id, ==, i);
      for (int j = 0; j < NBASES; j++) {
         g_assert_cmpint(dfa->states[i].next[j].state, ==, 0);
         g_assert_cmpint(dfa->states[i].next[j].match, ==, DFA_COMPUTE);
      }
   }
   g_assert_cmpint(dfa->size, ==, 1024);
}

void
test_dfa_step
(void)
{
   return;
}

void
test_parse
(void)
{

   char *keysp;
   g_assert_cmpint(parse("AaAaAaAa", &keysp), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keysp[i], ==, 1);
   }
   free(keysp);

   g_assert_cmpint(parse("CcCcCcCc", &keysp), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keysp[i], ==, 2);
   }
   free(keysp);

   g_assert_cmpint(parse("GgGgGgGg", &keysp), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keysp[i], ==, 4);
   }
   free(keysp);

   g_assert_cmpint(parse("TtTtTtTt", &keysp), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keysp[i], ==, 8);
   }
   free(keysp);

   g_assert_cmpint(parse("NnNnNnNn", &keysp), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keysp[i], ==, 31);
   }
   free(keysp);

   g_assert_cmpint(parse("Nn[]Nn[]NnN[]n", &keysp), ==, 8);
   for (int i = 0 ; i < 8 ; i++) {
      g_assert_cmpint(keysp[i], ==, 31);
   }
   free(keysp);


   g_assert_cmpint(parse("[GATC][gatc][GaTc][gAtC]", &keysp), ==, 4);
   for (int i = 0 ; i < 4 ; i++) {
      g_assert_cmpint(keysp[i], ==, 15);
   }
   free(keysp);

   g_assert_cmpint(parse("[GATCgatc", &keysp), ==, -1);
   free(keysp);

   g_assert_cmpint(parse("A]", &keysp), ==, -1);
   free(keysp);

   g_assert_cmpint(parse("[ATG[C]]", &keysp), ==, -1);
   free(keysp);

   g_assert_cmpint(parse("Z", &keysp), ==, -1);
   free(keysp);

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

   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/trie_new", test_trie_new);
   g_test_add_func("/trie_insert", test_trie_insert);
   g_test_add_func("/trie_search_getrow", test_trie_search_getrow);
   g_test_add_func("/trie_reset", test_trie_reset);
   g_test_add_func("/dfa_new", test_dfa_new);
   g_test_add_func("/dfa_newvertex", test_dfa_newvertex);
   g_test_add_func("/parse", test_parse);
   return g_test_run();
}
