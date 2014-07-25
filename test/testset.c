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
test_new_stack
(void)
{

   for (int i = 1 ; i < 10000 ; i++) {
      nstack_t *stack = new_stack(i);
      g_assert(stack != NULL);
      g_assert_cmpint(stack->p, ==, 0);
      g_assert_cmpint(stack->l, ==, i);
      free(stack);
   }

   for (int i = 0 ; i > -10000 ; i--) {
      nstack_t *stack = new_stack(i);
      g_assert(stack != NULL);
      g_assert_cmpint(stack->p, ==, 0);
      g_assert_cmpint(stack->l, ==, 1);
      free(stack);
   }

   return;

}


void
test_stack_add
(void)
{

   nstack_t *stack = new_stack(0);
   for (int i = 0 ; i < 10000 ; i++) {
      stack_add(&stack, i);
      g_assert(stack != NULL);
      g_assert_cmpint(stack->p, ==, i+1);
      g_assert_cmpint(stack->l, >=, i);
      g_assert_cmpint(stack->val[i], ==, i);
   }
   free(stack);

   return;

}


void
test_setactive
(void)
{

   char matrix[12] = {0};
   nstack_t *stack = new_stack(1);

   // Basic tests.
   setactive(3, 4, 0, 1, matrix, &stack);
   // Sets positions 0, 4 and 8.
   for (int i = 0 ; i < 12 ; i++) {
      g_assert_cmpint(matrix[i], ==, i % 4 == 0);
   }
   memset(matrix, 0, 12 * sizeof(char));

   setactive(4, 3, 0, 1, matrix, &stack);
   // Sets positions 0, 5 and 10.
   for (int i = 0 ; i < 12 ; i++) {
      g_assert_cmpint(matrix[i], ==, i % 5 == 0);
   }
   memset(matrix, 0, 12 * sizeof(char));

   // Test 'status' values.
   setactive(4, 3, 3, 255, matrix, &stack);
   // Sets positions 3.
   for (int i = 0 ; i < 12 ; i++) {
      g_assert_cmpint(matrix[i], ==, i == 3 ? -1 : 0);
   }
   memset(matrix, 0, 12 * sizeof(char));


   // Test on a large matrix.
   char *bigmatrix = calloc(10000*10000, sizeof(char));
   g_assert(bigmatrix != NULL);
   setactive(10000, 10000, 0, 1, bigmatrix, &stack);
   for (int i = 0 ; i < 10000*10000 ; i++) {
      g_assert_cmpint(bigmatrix[i], ==, i % 10001 == 0);
   }
   memset(bigmatrix, 0, 10000*10000 * sizeof(char));
   setactive(10000, 10000, 1, 1, bigmatrix, &stack);
   for (int i = 0 ; i < 10000*10000 ; i++) {
      g_assert_cmpint(bigmatrix[i], ==, i % 10001 == 1);
   }

   free(bigmatrix);
   free(stack);

   return;

}


void
test_trie_new_free
(void)
{

   for (int i = 1 ; i < 100 ; i++) {
   for (int j = 1 ; j < 100 ; j++) {
      btrie_t *trie = trie_new(i*100, j*100);
      g_assert(trie != NULL);
      g_assert(trie->root != NULL);
      g_assert_cmpint(trie->pos, ==, 1);
      g_assert_cmpint(trie->size, ==, i*100);
      g_assert_cmpint(trie->height, ==, j*100);
      trie_free(trie);
   }
   }

   for (int i = 0 ; i > -100 ; i--) {
   for (int j = 0 ; j > -100 ; j--) {
      btrie_t *trie = trie_new(i*100, j*100);
      g_assert(trie != NULL);
      g_assert(trie->root != NULL);
      g_assert_cmpint(trie->pos, ==, 1);
      g_assert_cmpint(trie->size, ==, 1);
      g_assert_cmpint(trie->height, ==, 1);
      trie_free(trie);
   }
   }

   return;

}


void
test_trie_insert
(void)
{

   btrie_t *trie = trie_new(1,10);
   g_assert(trie != NULL);

   bnode_t bnode = { .next = {0,0} };

   // Insert leftmost branch in the trie.
   trie_insert(trie, "\0\0\0\0\0\0\0\0\0\0", 4897892);
   g_assert_cmpint(trie->pos, ==, 10);
   g_assert_cmpint(trie->size, ==, 16);
   for (int i = 0 ; i < 9 ; i++) {
      bnode_t bnode = trie->root[i];
      g_assert_cmpint(bnode.next[0], ==, i+1);
   }
   bnode = trie->root[0];
   g_assert_cmpint(bnode.next[0], ==, 1);
   g_assert_cmpint(bnode.next[1], ==, 0);
   bnode = trie->root[9];
   g_assert_cmpint(bnode.next[0], ==, 4897892);
   g_assert_cmpint(bnode.next[1], ==, 0);

   // Insert rightmost branch in the trie.
   trie_insert(trie, "\1\1\1\1\1\1\1\1\1\1", 22923985);
   g_assert_cmpint(trie->pos, ==, 19);
   g_assert_cmpint(trie->size, ==, 32);
   for (int i = 1 ; i < 9 ; i++) {
      bnode_t bnode = trie->root[i];
      g_assert_cmpint(bnode.next[0], ==, i+1);
      g_assert_cmpint(bnode.next[1], ==, 0);
   }
   for (int i = 10 ; i < 18 ; i++) {
      bnode_t bnode = trie->root[i];
      g_assert_cmpint(bnode.next[0], ==, 0);
      g_assert_cmpint(bnode.next[1], ==, i+1);
   }
   bnode = trie->root[0];
   g_assert_cmpint(bnode.next[0], ==, 1);
   g_assert_cmpint(bnode.next[1], ==, 10);
   bnode = trie->root[9];
   g_assert_cmpint(bnode.next[0], ==, 4897892);
   g_assert_cmpint(bnode.next[1], ==, 0);
   bnode = trie->root[18];
   g_assert_cmpint(bnode.next[0], ==, 0);
   g_assert_cmpint(bnode.next[1], ==, 22923985);

   // Insert middle branch in the trie.
   trie_insert(trie, "\1\0\1\0\1\0\1\0\1\0", 9309871);
   g_assert_cmpint(trie->pos, ==, 27);
   g_assert_cmpint(trie->size, ==, 32);
   for (int i = 1 ; i < 9 ; i++) {
      bnode_t bnode = trie->root[i];
      g_assert_cmpint(bnode.next[0], ==, i+1);
      g_assert_cmpint(bnode.next[1], ==, 0);
   }
   for (int i = 11 ; i < 18 ; i++) {
      bnode_t bnode = trie->root[i];
      g_assert_cmpint(bnode.next[0], ==, 0);
      g_assert_cmpint(bnode.next[1], ==, i+1);
   }
   for (int i = 19 ; i < 26 ; i++) {
      bnode_t bnode = trie->root[i];
      if (i % 2 == 1) {
         g_assert_cmpint(bnode.next[0], ==, 0);
         g_assert_cmpint(bnode.next[1], ==, i+1);
      }
      else {
         g_assert_cmpint(bnode.next[0], ==, i+1);
         g_assert_cmpint(bnode.next[1], ==, 0);
      }
   }
   bnode = trie->root[0];
   g_assert_cmpint(bnode.next[0], ==, 1);
   g_assert_cmpint(bnode.next[1], ==, 10);
   bnode = trie->root[9];
   g_assert_cmpint(bnode.next[0], ==, 4897892);
   g_assert_cmpint(bnode.next[1], ==, 0);
   bnode = trie->root[10];
   g_assert_cmpint(bnode.next[0], ==, 19);
   g_assert_cmpint(bnode.next[1], ==, 11);
   bnode = trie->root[18];
   g_assert_cmpint(bnode.next[0], ==, 0);
   g_assert_cmpint(bnode.next[1], ==, 22923985);
   bnode = trie->root[26];
   g_assert_cmpint(bnode.next[0], ==, 9309871);
   g_assert_cmpint(bnode.next[1], ==, 0);

   trie_free(trie);

   btrie_t *lowtrie = trie_new(1,1);
   g_assert(lowtrie != NULL);

   // Insert a path longer than trie height.
   trie_insert(lowtrie, "\0\0\0", 1);
   g_assert_cmpint(lowtrie->pos, ==, 1);
   g_assert_cmpint(lowtrie->root->next[0], ==, 1);

   trie_free(lowtrie);

   return;

}


void
test_trie_search
(void)
{

   btrie_t *trie = trie_new(1,10);
   g_assert(trie != NULL);

   char path[10] = {0};
   // Insert random paths and search them.
   srand48(123);
   for (int i = 0 ; i < 10000 ; i++) {
      for (int j = 0 ; j < 10 ; j++) {
         path[j] = (drand48() < .5);
      }
      unsigned int value = (int) lrand48();
      trie_insert(trie, path, value);
      g_assert_cmpint(trie_search(trie, path), ==, value);
   }

   trie_free(trie);

   return;

}

 
void
test_trie_reset
(void)
{

   btrie_t *trie = trie_new(1,10);
   g_assert(trie != NULL);

   bnode_t bnode = { .next = {0,0} };

   // Insert leftmost branch in the trie.
   trie_insert(trie, "\0\0\0\0\0\0\0\0\0\0", 36636);
   g_assert_cmpint(trie->pos, ==, 10);
   g_assert_cmpint(trie->size, ==, 16);
   for (int i = 0 ; i < 9 ; i++) {
      bnode_t bnode = trie->root[i];
      g_assert_cmpint(bnode.next[0], ==, i+1);
   }
   bnode = trie->root[0];
   g_assert_cmpint(bnode.next[0], ==, 1);
   g_assert_cmpint(bnode.next[1], ==, 0);
   bnode = trie->root[9];
   g_assert_cmpint(bnode.next[0], ==, 36636);
   g_assert_cmpint(bnode.next[1], ==, 0);

   trie_reset(trie);
   g_assert(trie != NULL);
   g_assert_cmpint(trie->pos, ==, 0);
   g_assert_cmpint(trie->size, ==, 16);
   for (int i = 0 ; i < 16 ; i++) {
      bnode_t bnode = trie->root[i];
      g_assert_cmpint(bnode.next[0], ==, 0);
      g_assert_cmpint(bnode.next[1], ==, 0);
   }

   trie_free(trie);

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

   g_assert_cmpint(parse("[GATC][gatc][GaTc][gAtC]", &keysp), ==, 4);
   for (int i = 0 ; i < 4 ; i++) {
      g_assert_cmpint(keysp[i], ==, 15);
   }
   free(keysp);

   g_assert_cmpint(parse("[GATCgatc", &keysp), ==, -1);
   free(keysp);

   g_assert_cmpint(parse("A]", &keysp), ==, -1);
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
   g_test_add_func("/new_stack", test_new_stack);
   g_test_add_func("/stack_add", test_stack_add);
   g_test_add_func("/setactive", test_setactive);
   g_test_add_func("/trie_new_free", test_trie_new_free);
   g_test_add_func("/trie_insert", test_trie_insert);
   g_test_add_func("/trie_search", test_trie_search);
   g_test_add_func("/trie_reset", test_trie_reset);
   g_test_add_func("/parse", test_parse);
   return g_test_run();
}
