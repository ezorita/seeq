#include "seeq.h"

int main(int argc, char *argv[])
{
   if (argc != 3) {
      fprintf(stderr,"usage: seeq expression distance inputfile\n");
      exit(0);
   }

   char * expression = argv[1];
   int    tau = atoi(argv[2]);
   FILE * in = fopen(argv[3], "r");
   
   if (in == NULL) {
      fprintf(stderr,"error: could not open file\n");
      exit(0);
   }
   
   char * keys;
   int    wlen = parse(expression, keys);
   int    nnfa = 2*tau + 1;

   nfa_t * nfa = malloc (nnfa*sizeof(nfa_t));
   for (int i = 0; i < nnfa; i++) {
      nfa[i].rows  = tau + 1;
      nfa[i].cols  = wlen + 1;
      nfa[i].exp   = keys;
      nfa[i].act   = calloc(nfa.rows*nfa.cols, sizeof(int));
      nfa[i].stack = new_stack(INITIAL_STACK_SIZE);
      nfa[i].temp  = new_stack(INITIAL_STACK_SIZE);
      nfa[i].mbuf  = malloc(sizeof(mbuf_t) + nnfa*sizeof(int));
      nfa[i].mbuf->p = nnfa - i;
   }

   // Read and update
   char * line = malloc(INITIAL_LINE_SIZE*sizeof(char));
   size_t size;
   int status = 1;
   while (getline(&line, &size, in) != -1) {
      line[size] = 0;
      for (int i = 0; i < size; i++) {
         // Activate NFA.
         setactive(nfa[i%nnfa], 0, status);
         
      }
      stack_clear();
   }

   return 0;
}

void
update
(
 nfa_t * nfa,
 char    value,
 int     satus
)
{
   int rows = nfa->rows;

   // Swap stacks
   nstack_t * active = nfa->temp;
   nstack_t * temp = nfa->active;
   nfa->stack = active;
   nfa->temp  = temp;

   // Update active nodes
   for (int i = 0; i < active->p; i++) {
      int node = stack->val[i];
      if (nfa->exp[node/rows] & value > 0) setactive(nfa, node + rows);
      else {
         setactive(nfa, node + 1, status);
         setactive(nfa, node + rows + 1, status);
      }
   }

   // Detect match. (If there are multiple matches, take the min distance match)
   mbuf_t * buf = nfa->mbuf;
   int tau = rows - 1;
   int m = 0;
   int offset = rows * (nfa->cols - 1);
   for (; m < rows; m++) if (nfa->act[offset + m] == status) break;
   buf->p = (buf->p + 1) % (2*tau+1);
   buf->m[buf->p] = m;
}


void
setactive
(
 nfa_t * nfa,
 int     node,
 int     status
)
{
   int * active = nfa->act;
   int rows = nfa->rows;
   if (active[node] == status) return;
   active[node] = status;
   // Do not add matches to list.
   if (node < nfa->rows * (nfa->cols - 1)) stack_add(nfa->temp, node);
   int free = node + rows + 1;
   if (free < rows * nfa->cols) setactive(nfa, free, status);
}


nstack_t *
new_stack
(
 int elements
)
{
   nstack_t * stack = malloc(sizeof(nstack_t) + elements*sizeof(int));
   if (stack == NULL) {
      fprintf(stderr, "error allocating nstack_t\n");
      exit(1);
   }
   stack->p = 0;
   stack->l = elements;
   return stack;
}

void
stack_add
(
 nstack_t ** stackp,
 int         value
)
{
   nstack_t * stack = *stackp;
   if (stack->p >= stack->l) {
      size_t newsize = 2*stack->l;
      *stackp = stack = realloc(stack, sizeof(nstack_t)+newsize*sizeof(int));
      if (stack == NULL) {
         fprintf(stderr, "error reallocating nstack\n");
         exit(1);
      }
   }
   
   stack->val[stack->p++] = value;
}
