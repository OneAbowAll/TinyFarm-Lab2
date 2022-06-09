//Master Worker
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "xerrori.h"

#define QUI __LINE__, __FILE__
#define MAX_NAME_LENGTH 255

#define HOST "127.0.0.1"
#define PORT 65432

/*
int getopt(int argc, char * const argv[],
          const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;

#include <getopt.h>
*/

typedef struct{  
  char** files;
  int *cIndex;

  int qlen;
  int delay;

  pthread_mutex_t *mutex;
  sem_t *sem_free;
  sem_t *sem_items;
} dati;

void *worker(void *arg);
void intHandler(int s);
void freeMemory(dati d);

ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);

volatile sig_atomic_t stop = 0;
//void logBuffer(char** buffer, int qlen, int cIndex, int pIndex, sem_t *free, sem_t *item);

int main(int argc, char *argv[])
{
	//Gestione segnali
	struct sigaction sa;

	sigaction(SIGINT, NULL, &sa);
	sa.sa_handler = &intHandler;
	sigaction(SIGINT, &sa, NULL);
	
  #pragma region Input-Management

  int opt;
  int nthread = 4, qlen = 8, delay = 0;
	
  //Prendi argomenti opzionali
  while((opt = getopt(argc, argv, "n:q:t:")) != -1){
    switch (opt)
    {
    case 'n':
      nthread = max(atoi(optarg), 1);
      break;
     
    case 'q':
      qlen =  max(atoi(optarg), 1);
      break;
    
    case 't':
      delay =  max(atoi(optarg), 0)*1000;
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

	//Connessione al server
	struct sockaddr_in serv;
	int skt = 0; //File descript. del socket

	if((skt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		termina("Errore creazione socket");
	
	serv.sin_family = AF_INET; //assegna indirizzo
	serv.sin_port = htonl(PORT);
	serv.sin_addr.s_addr = inet_addr(HOST);

	if(connect(skt, (struct sockaddr*)&serv, sizeof(serv)) < 0)
		termina("Errore apertura connessione");	
	else
		fprintf(stdout, "Connessione creata con successo!!\n");

	
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
  shm.delay = delay;

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
		if(stop) break;
		
    xsem_wait(&sem_free, QUI);
		
    strcpy(files[pIndex++ % qlen], argv[i]);
    fprintf(stdout, "Scritto nel buffer %s \n", files[(pIndex-1) % qlen]);

    xsem_post(&sem_items, QUI);

		usleep(delay);
  }

	for (int i = 0; i < nthread; i++)
  {
    xsem_wait(&sem_free, QUI);
		
    strcpy(files[pIndex++ % qlen], "-1");
    fprintf(stdout, "Scritto nel buffer %s \n", files[(pIndex-1) % qlen]);
		
    xsem_post(&sem_items, QUI);
		
		//usleep(delay);
  }
	
  for (int j = 0; j < nthread; j++)
  {
    fprintf(stdout, "Joinato il Thread %ld. \n", t[j]);
    xpthread_join(t[j], NULL, QUI);
  }

	freeMemory(shm);
	return 0;
}

void freeMemory(dati d){
	/*Libero lo spazio allocato alle singole stringhe 
		e poi libero lo spazio per l'array stesso */
	
	for(int i=0; i<d.qlen; i++)
		free(d.files[i]);
	free(d.files);

	exit(1);

	/*Onestamente non so se sia il metodo giusto per fare questa cosa 
		ma per me ha senso e valgrind conferma */
}

void intHandler(int s){
	if(stop)
		printf("\n== Segnale SIGINT == Procedura di chiusura in corso, attendere...\n");
	else
		printf("\n== Segnale SIGINT == Procedura di chiusura iniziata...\n");
	
	stop = 1;
}

void *worker(void *arg){ 
  dati *d = (dati *)arg;
	
	int f;
	char *nomeFile = "-1";
	long sum = 0;
	
	while(true){
  	xsem_wait(d->sem_items, QUI);
  	xpthread_mutex_lock(d->mutex, QUI);
		
		nomeFile = d->files[*(d->cIndex) % d->qlen];
		*(d->cIndex) += 1;

  	xpthread_mutex_unlock(d->mutex, QUI);
  	xsem_post(d->sem_free, QUI);

		if(strcmp(nomeFile, "-1") == 0) break;
		
		f = open(nomeFile, O_RDONLY);
		ssize_t e;
		
		sum = 0; int cont = 0; long n;
		while(true){
			e = read(f, &n, sizeof(long));
			if(e != sizeof(long)) break;
			
			sum += n * cont;
			cont += 1;
		}
		xclose(f, QUI);

		fprintf(stdout, "Sono un thread qualsiasi e ho calcolcato che il file %s contiene la somma %ld. \n", nomeFile, sum);
		
		usleep(d->delay);
	}

  pthread_exit(NULL);
}


/* Read "n" bytes from a descriptor */
ssize_t readn(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nread;
 
   nleft = n;
   while (nleft > 0) {
     if((nread = read(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount read so far */
     } else if (nread == 0) break; /* EOF */
     nleft -= nread;
     ptr   += nread;
   }
   return(n - nleft); /* return >= 0 */
}


/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nwritten;
 
   nleft = n;
   while (nleft > 0) {
     if((nwritten = write(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount written so far */
     } else if (nwritten == 0) break; 
     nleft -= nwritten;
     ptr   += nwritten;
   }
   return(n - nleft); /* return >= 0 */
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