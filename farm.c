//Master Worker
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int getopt(int argc, char * const argv[],
          const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;

#include <getopt.h>

int main(int argc, char *argv[])
{
  int opt;
  int nthread = 4, qlen = 8, delay = 0;

  // controlla numero argomenti
  if(argc<2) {
      printf("Uso: %s file [file ...] \n",argv[0]);
      //return 1;
  }
	
  //Prendi argomenti opzionali
  while((opt = getopt(argc, argv, "n:q:t:")) != -1){
    switch (opt)
    {
    case 'n':
      nthread =  atoi(optarg);
      break;
     
    case 'q':
      qlen =  atoi(optarg);
      break;
    
    case 't':
      delay =  atoi(optarg);
      break;

    default: /* Caso ? */
      fprintf(stdout, "Parametro opzionale errato. Uso: %s file [file...] [-n nthread] [-q qlen] [-t delay].\n", argv[0]);
      break;
    }
  }

  fprintf(stdout, "Parametri attuali => nthread: %d; qlen: %d; delay = %d \n", nthread, qlen, delay);
	return 0;
}