#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
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
      matrix[i] = 0;
   }

   setactive(4, 3, 0, 1, matrix, &stack);
   // Sets positions 0, 5 and 10.
   for (int i = 0 ; i < 12 ; i++) {
      g_assert_cmpint(matrix[i], ==, i % 5 == 0);
      matrix[i] = 0;
   }

   // Test 'status' values.
   setactive(4, 3, 3, 255, matrix, &stack);
   // Sets positions 3.
   for (int i = 0 ; i < 12 ; i++) {
      g_assert_cmpint(matrix[i], ==, i == 3 ? -1 : 0);
      matrix[i] = 0;
   }


   // Test on a large matrix.
   char *bigmatrix = calloc(10000*10000, sizeof(char));
   g_assert(bigmatrix != NULL);
   setactive(10000, 10000, 0, 1, bigmatrix, &stack);
   for (int i = 0 ; i < 10000*10000 ; i++) {
      g_assert_cmpint(bigmatrix[i], ==, i % 10001 == 0);
      bigmatrix[i] = 0;
   }
   setactive(10000, 10000, 1, 1, bigmatrix, &stack);
   for (int i = 0 ; i < 10000*10000 ; i++) {
      g_assert_cmpint(bigmatrix[i], ==, i % 10001 == 1);
      bigmatrix[i] = 0;
   }

   free(bigmatrix);
   free(stack);

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
   return g_test_run();
}
