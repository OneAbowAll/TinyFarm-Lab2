//Master Worker
#include <unistd.h>
#include <sys/types.h>

#include "xerrori.h"

#define QUI __LINE__, __FILE__
#define MAX_NAME_LENGTH 255

int getopt(int argc, char * const argv[],
          const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;

#include <getopt.h>

typedef struct{  
  char** files;
  int *cIndex;
  int qlen;

  pthread_mutex_t *mutex;
  sem_t *sem_free;
  sem_t *sem_items;
} dati;

void *worker(void *arg);

//void logBuffer(char** buffer, int qlen, int cIndex, int pIndex, sem_t *free, sem_t *item);

int main(int argc, char *argv[])
{

  #pragma region Input-Management

  int opt;
  int nthread = 4, qlen = 8, delay = 0;
	
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
      fprintf(stdout, "Parametro opzionale errato. Uso: %s file [file...] [-n nthread] [-q qlen] [-t delay] \n", argv[0]);
      break;
    }
  }

  // controlla numero argomenti, ricorda che una volta usato getopt i parametri non opzionali (i file) sono stati messi in fondo
  if(optind >= argc) {
      fprintf(stdout, "Specificare almeno un file. Uso: %s file [file...] [-n nthread] [-q qlen] [-t delay] \n",argv[0]);
      return 1;
  }

  fprintf(stdout, "Parametri attuali => nthread: %d; qlen: %d; delay = %d \n", nthread, qlen, delay);

  #pragma endregion

  //Gestione sistema produttori consumatori
  sem_t sem_free, sem_items;
  xsem_init(&sem_free, 0, qlen, QUI); //Contatore di spazio livero
  xsem_init(&sem_items, 0, 0, QUI); //Contatore di oggetti nel buffer

  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //Assicura che i Workers non si "pestino i piedi" tra di loro 
  
  char** files; //Buffer dei nomi file
	files = malloc(sizeof(char*)*qlen);
	for(int i=0; i<qlen; i++)
			files[i] = malloc(sizeof(char)*MAX_NAME_LENGTH);
	
	
  int pIndex = 0, cIndex = 0;

  pthread_t t[nthread];
  dati shm;

  shm.files = files;
  shm.cIndex = &cIndex;
  shm.qlen = qlen;

  shm.mutex = &mutex;
  shm.sem_free = &sem_free;
  shm.sem_items = &sem_items;

  for (int i = 0; i < nthread; i++)
  {
    xpthread_create(&t[i], NULL, worker, &shm, QUI);
    fprintf(stdout, "Creato Thread %ld. \n", t[i]);
  }
	
  //Inserisci nel buffer tutti i 
  for (int i = optind; i < argc; i++)
  {		
    xsem_wait(&sem_free, QUI);
    strcpy(files[pIndex++ % qlen], argv[i]);
    fprintf(stdout, "Scritto nel buffer %s \n", files[(pIndex-1) % qlen]);

    xsem_post(&sem_items, QUI);
  }

	for (int i = 0; i < nthread; i++)
  {
    xsem_wait(&sem_free, QUI);
    strcpy(files[pIndex++ % qlen], "-1");
    fprintf(stdout, "Scritto nel buffer %s \n", files[(pIndex-1) % qlen]);
		
    xsem_post(&sem_items, QUI);
  }
	
  for (int j = 0; j < nthread; j++)
  {
    fprintf(stdout, "Joinato il Thread %ld. \n", t[j]);
    xpthread_join(t[j], NULL, QUI);
  }

	return 0;
}

void *worker(void *arg){ 
  dati *d = (dati *)arg;

	char *nomeFile = "-1";
	do{
  	xsem_wait(d->sem_items, QUI);
  	xpthread_mutex_lock(d->mutex, QUI);

		nomeFile = d->files[*(d->cIndex) % d->qlen];
		*(d->cIndex) += 1;

		fprintf(stdout, "Salve un thread qualsiasi e ho appena letto il file %s \n", nomeFile);

		
  	xpthread_mutex_unlock(d->mutex, QUI);
  	xsem_post(d->sem_free, QUI);
	}while(strcmp(nomeFile, "-1") != 0);

  pthread_exit(NULL);
}

/*
void logBuffer(char** buffer, int qlen, int cIndex, int pIndex, sem_t *free, sem_t *item){
	FILE *f = fopen("bufferLog.txt", "a");
	
	fprintf(f, "\n{\n");
	if(pIndex == -1)
		fprintf(f, "\tConsumatore \n");
	else
		fprintf(f, "\tProduttore\n");
	
	int sFree, sItems;
	sem_getvalue(free, &sFree);
	sem_getvalue(item, &sItems);
	
	fprintf(f, "\t Sem Free => %d\n", sFree);
	fprintf(f, "\t Sem Items => %d\n", sItems);
	fprintf(f, "\t cIndex => %d\n", cIndex % qlen);
	fprintf(f, "\t pIndex => %d\n", pIndex % qlen);
	fprintf(f, "\n");
	for (int i = 0; i < qlen; i++)
  {
		fprintf(f, "\t [%d] => %s", i, buffer[i]);
		
		if(cIndex % qlen == i)
			fprintf(f, " <-- cIndex");

		if(pIndex % qlen == i)
			fprintf(f, " <-- pIndex");

		
		fprintf(f, "\n");
  }
	fprintf(f, "}\n");
	fclose(f);
}*/