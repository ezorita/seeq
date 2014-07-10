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

   nfa_t * nfa = malloc ((2*tau+1)*sizeof(nfa_t));
   for (int i = 0; i < 2*tau+1; i++) {
      nfa[i].rows = tau + 1;
      nfa[i].cols = wlen;
      nfa[i].exp  = keys;
      nfa[i].act  = calloc(nfa.rows*nfa.cols, sizeof(int));
   }

   // Read and update
   while (readline(

   return 0;
}

void
update
(
 nfa_t * nfa,
 int     node,
 char    value,
 int     satus
)
{
   int rows = nfa->rows;
   if (nfa->exp[node/rows] & value > 0) setactive(nfa, node + rows);
   else {
      setactive(nfa, node + 1, status);
      setactive(nfa, node + rows + 1, status);
   }
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
   int free = node + rows + 1;
   if (free < rows * nfa->cols) setactive(nfa, free, status);
}
