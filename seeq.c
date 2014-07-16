#include "seeq.h"

void SIGSEGV_handler(int sig) {
   void *array[10];
   size_t size;

   // get void*'s for all entries on the stack
   size = backtrace(array, 10);

   // print out all the frames to stderr
   fprintf(stderr, "Error: signal %d:\n", sig);
   backtrace_symbols_fd(array, size, 2);
   exit(1);
}



int main(int argc, char *argv[])
{
   signal(SIGSEGV, SIGSEGV_handler); 

   if (argc != 4) {
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
   int    wlen = parse(expression, &keys);
   int    nnfa = 2*tau + 1;

   nfa_t * nfa = malloc (nnfa*sizeof(nfa_t));
   for (int i = 0; i < nnfa; i++) {
      nfa[i].rows  = tau + 1;
      nfa[i].cols  = wlen + 1;
      nfa[i].exp   = keys;
      nfa[i].act   = calloc(nfa[i].rows*nfa[i].cols, sizeof(int));
      nfa[i].stack = new_stack(INITIAL_STACK_SIZE);
      nfa[i].temp  = new_stack(INITIAL_STACK_SIZE);
   }
   int * mbuffer = malloc(nnfa*sizeof(int));
   for (int i = 0; i < nnfa; i++) mbuffer[i] = tau + 1;

   // Read and update
   char * line = malloc(INITIAL_LINE_SIZE*sizeof(char));
   size_t bsize = INITIAL_LINE_SIZE;
   ssize_t size;
   int status = 1, lines = 0;
   int align  = (nnfa - ((wlen-tau)%nnfa)) % nnfa;
   while ((size = getline(&line, &bsize, in)) != -1) {
      lines++;
      line[size] = 0;
      if (size < wlen - tau) continue;
      int dist, offset, post_match = 0;
      int start_dist = tau+1, start_pos = 0, start_offset = tau + 1;
      for (int i = 0; i < size; i++) {
         // Activate NFA.
         setactive(nfa + i%nnfa, 0, status++);
         // Update automatas.
         for (int j = 0; j < nnfa; j++) {
            int d = update(nfa + j, line[i], status);
            mbuffer[(i+1-j+align)%nnfa] = d;
         }
         /*
         // DEBUG MACHINES
         fprintf(stdout, "-- character %d: %c\n", i, line[i]);
         for (int j = 0; j < nnfa; j++) {
            nfa_t * mac = nfa + j;
            fprintf(stdout,"NFA(%d):\n",j);
            for (int r = 0; r < mac->rows; r++) {
               for (int c = 0; c < mac->cols; c++) {
                  fprintf(stdout, " %d", mac->act[c*nfa->rows + r]);
               }
               fprintf(stdout, "\n");
            }
         }
         fprintf(stdout, "match = [%d %d %d]\n", mbuffer[0], mbuffer[1], mbuffer[2]);
         */

         // Match filter.
         proc_match(mbuffer, nnfa, &dist, &offset);
         if (start_dist < dist) {
            if (post_match == 1) {
               start_pos    = i;
               start_dist   = dist;
               start_offset = offset;
            } else {
               fprintf(stdout, "%d,%d:%d+%d (%d)\n", lines, start_pos-wlen+1-start_offset, start_pos, i-1-start_pos, start_dist);
               // break if only first match is desired
               //break;
               post_match = 1;
               start_offset = tau + 1;
               
            }
            
         } else if (start_dist == dist && dist <= tau) {
            if (start_offset*start_offset > offset*offset) {
               start_pos    = i;
               start_offset = offset;
            }
            post_match = 0;
         } else {
            start_pos    = i;
            start_dist   = dist;
            start_offset = offset;
            post_match   = 0;
         }
         //printf(stdout, "start_i = %d\tstart_d = %d\tstart_o = %d\n", start_pos, start_dist, start_offset);
      }

      // Report last match if start_dist == dist.
      if (start_dist == dist && dist <= tau) {
         // TODO.
      }

      // Clear active nodes.
      for (int i = 0; i < nnfa; i++) {
         nfa[i].stack->p = 0;
         nfa[i].temp->p  = 0;
      }
   }

   return 0;
}


int
parse
(
 char * expr,
 char ** keysp
)
{
   *keysp = calloc(strlen(expr),sizeof(char));
   char * keys = *keysp;
   int i = 0;
   int l = 0;
   int add = 0;
   char c;
   while((c = expr[i]) != 0) {
      if      (c == 'A' || c == 'a') keys[l] |= 0x01;
      else if (c == 'C' || c == 'c') keys[l] |= 0x02;
      else if (c == 'G' || c == 'g') keys[l] |= 0x04;
      else if (c == 'T' || c == 't') keys[l] |= 0x08;
      else if (c == 'N' || c == 'n') keys[l] |= 0x1F;
      else if (c == '[') add = 1;
      else if (c == ']') add = 0;
      
      if (!add) l++;
      i++;
   }
   
   if (add == 1) return -1;
   else return l;
}


void
proc_match
(
 int * mbuf,
 int   nnfa,
 int * dist,
 int * off
)
{
   int tau = (nnfa-1)/2;
   int offset = -tau;

   *dist = mbuf[0];
   *off  = offset;
   
   for (int i = 1; i < nnfa; i++) {
      offset++;
      if (mbuf[i] < *dist) {
         *dist = mbuf[i];
         *off  = offset;
      } else if (mbuf[i] == *dist) {
         if ((*off) * (*off) > offset * offset)
            *off = offset;
      }
   }
}


int
update
(
 nfa_t * nfa,
 char    value,
 int     status
)
{
   int rows = nfa->rows;

   if      (value == 'A' || value == 'a') value = 0x01;
   else if (value == 'C' || value == 'c') value = 0x02;
   else if (value == 'G' || value == 'g') value = 0x04;
   else if (value == 'T' || value == 't') value = 0x08;
   else if (value == 'N' || value == 'n') value = 0x10;
   else value = 0x00;

   // Swap stacks
   nstack_t * active = nfa->temp;
   nstack_t * temp = nfa->stack;
   nfa->stack = active;
   nfa->temp  = temp;

   // Update active nodes
   for (int i = 0; i < active->p; i++) {
      int node = active->val[i];
      if ((nfa->exp[node/rows] & value) > 0)
         setactive(nfa, node + rows, status);
      else if (node % rows < rows - 1) {
         setactive(nfa, node + 1, status);
         setactive(nfa, node + rows + 1, status);
      }
   }

   // Reset list.
   active->p = 0;

   // Detect match and return.
   // If there are multiple matches, take the min distance match.
   int tau = rows - 1;
   int m = 0;
   int offset = rows * (nfa->cols - 1);
   for (; m < rows; m++) if (nfa->act[offset + m] == status) break;
   return m;
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
   if (node < nfa->rows * (nfa->cols - 1)) stack_add(&(nfa->temp), node);
   // Last row line, return.
   if (node % rows == rows - 1) return;
   // Epsilon.
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
