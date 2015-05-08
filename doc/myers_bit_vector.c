#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t bitV_t;

static const char SIGMA[4] = "ACGT";
static const int TRANSLATE[256] = {
   ['a'] = 1, ['c'] = 2, ['g'] = 3, ['t'] = 4,
   ['A'] = 1, ['C'] = 2, ['G'] = 3, ['T'] = 4,
};

int
search
(
   const int m,
   bitV_t Peq[],
   char *txt,
   int k
)
{
   bitV_t Pv = ~0;
   bitV_t Mv =  0;
   int score = m;

   for (int j = 0 ; txt[j] != 0 ; j++) {
      bitV_t Eq = Peq[txt[j]];
      bitV_t Xv = Eq | Mv;
      bitV_t Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
      bitV_t Ph = Mv | ~ (Xh | Pv);
      bitV_t Mh = Pv & Xh;

      if (Ph & (1 << m-1)) {
         score++;
      }
      else if (Mh & (1 << m-1)) {
         score--;
      }

      Ph = Ph << 1;
      Mh = Mh << 1;
      Pv = Mh | ~ (Xv | Ph);
      Mv = Ph & Xv;

      if (score < k+1) {
         fprintf(stdout, "%d\n", j);
      }
   }

}

int main(int argc, char *argv[]) {

   bitV_t Peq[5] = {0,0,0,0,0};
   char query[] = "GATGTAGCGCGATTAGCCTGAAAATGCGAGTACGGCGCGAAT";
   int m = strlen(query);
   // Pre-compute 'Peq'.
   for (int s = 0 ; s < strlen(SIGMA) ; s++) {
      char t = SIGMA[s];
      for (int i = 0 ; i < m ; i++) {
         if (query[i] == t) Peq[s+1] = Peq[s+1] | (1 << i);
      }
   }

   char *seq = NULL;
   ssize_t nread;
   size_t buffer_size = 256; 
   char *line = malloc(buffer_size * sizeof(char));

   FILE *inputf = fopen(argv[1], "r");
   if (inputf == NULL) abort();

   while ((nread = getline(&line, &buffer_size, inputf)) != -1) {
      for (int j = 0 ; j < strlen(line) ; j++) {
         line[j] = TRANSLATE[line[j]];
      }
      //printf("%d\n", search(m, Peq, line, 12));
      search(m, Peq, line, 12);
   }

   fclose(inputf);
   free(line);
   return 0;

}
