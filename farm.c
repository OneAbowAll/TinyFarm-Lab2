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
#define PORT 65434

#define DEBUG_THRD 0
#define DEBUG_BUFF 1

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
void closeServer();
void freeMemory(dati d);

ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);

volatile sig_atomic_t stop = 0;
//void logBuffer(char** buffer, int qlen, int cIndex, int pIndex, sem_t *free, sem_t *item);

int main(int argc, char *argv[])
{
  printf("asss %ld o %ld \n\n", sizeof(long int), sizeof(long));

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

  fprintf(stdout, "Parametri attuali => nthread: %d; qlen: %d; delay = %d \n", nthread, qlen, delay/1000);

  #pragma endregion

  #pragma region Socket-Connection

        //Gestione segnali
        struct sigaction sa;

        sigaction(SIGINT, NULL, &sa);
        sa.sa_handler = &intHandler;
        sigaction(SIGINT, &sa, NULL);

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

    if(DEBUG_THRD)
      fprintf(stdout, "Creato Thread %ld. \n", t[i]);
  }

  //Inserisci nel buffer tutti i
  for (int i = optind; i < argc; i++)
  {
                if(stop) break;

    xsem_wait(&sem_free, QUI);

    strcpy(files[pIndex++ % qlen], argv[i]);

    if(DEBUG_BUFF)
      fprintf(stdout, "Scritto nel buffer %s \n", files[(pIndex-1) % qlen]);

    xsem_post(&sem_items, QUI);

                usleep(delay);
  }

        for (int i = 0; i < nthread; i++)
  {
    xsem_wait(&sem_free, QUI);

    strcpy(files[pIndex++ % qlen], "-1");
    if(DEBUG_BUFF)
      fprintf(stdout, "Scritto nel buffer %s \n", files[(pIndex-1) % qlen]);

    xsem_post(&sem_items, QUI);

                //usleep(delay);
  }

  for (int j = 0; j < nthread; j++)
  {
    xpthread_join(t[j], NULL, QUI);

    if(DEBUG_THRD)
      fprintf(stdout, "Joinato il Thread %ld. \n", t[j]);
  }

  closeServer();
        freeMemory(shm);
        return 0;
}

void closeServer(){
        //Connessione al server
        struct sockaddr_in serv;
        int skt = 0; //File descript. del socket

        if((skt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                termina("Errore creazione socket");

        serv.sin_family = AF_INET; //assegna indirizzo
        serv.sin_port = htons(PORT);
        serv.sin_addr.s_addr = inet_addr(HOST);

        if(connect(skt, (struct sockaddr*)&serv, sizeof(serv)) < 0)
                termina("Errore apertura connessione");
  else
                fprintf(stdout, "Aperta connesione con %s sulla porta %d ...\n", HOST, PORT);

  writen(skt, "!", 1);
}

void freeMemory(dati d){
        /*Libero lo spazio allocato alle singole stringhe
                e poi libero lo spazio per l'array stesso */

        for(int i=0; i<d.qlen; i++)
                free(d.files[i]);
        free(d.files);

        //exit(1);

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

        //Connessione al server
        struct sockaddr_in serv;
        int skt = 0; //File descript. del socket

        if((skt = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                termina("Errore creazione socket");

        serv.sin_family = AF_INET; //assegna indirizzo
        serv.sin_port = htons(PORT);
        serv.sin_addr.s_addr = inet_addr(HOST);

        if(connect(skt, (struct sockaddr*)&serv, sizeof(serv)) < 0){
                termina("Errore apertura connessione");
  }else
                fprintf(stdout, "Aperta connesione con %s sulla porta %d ...\n", HOST, PORT);

        int f;
        char *nomeFile = "-1";
        long sum = 0; long tmp;

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

                if(DEBUG_BUFF)
      fprintf(stdout, "Sono un thread qualsiasi e ho calcolcato che il file %s contiene la somma %ld. \n", nomeFile, sum);

    //Avverto il server che sto inviando dei dati
    writen(skt, "?", 1);

    //Invio primo i primi 4 bytes della somma e successivamnte i bytes rimanenti
    //Per capire il perchè di questa scelta scendere a fine file (*)
    tmp = htonl(sum >> 32);
    writen(skt, &tmp, 4);
    tmp = htonl(sum);
    writen(skt, &tmp, 4);

    //Invio Nome File
    writen(skt, nomeFile, MAX_NAME_LENGTH);

                usleep(d->delay);
        }

  if(close(skt)<0)
    perror("Errore chiusura socket");

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


/*
                                  ======= PROMEMORIA PER IL DAVIDE DEL FUTURO =======

  (*) Perchè ho inviato la variabile sum con due messaggi diversi?

      htonl() restituisce un unsigned int da 32 bit, questo significa che un long
      in c, che è da 64 bit, non viene inviato del tutto (non so bene se il numero veniva troncato al 32-esimo bit
      o se veniva fatto arrotondato al max che valore che poteva avere uint_32, testando su python mi sembra che venga arrotondato).

      Quindi quando su python provavo a chiedere per 8 byte da convertire in un long long (perchè la struct di python per un long usa solo 4 byte)
      ricevava sostanzialmente roba senza senso.
      La soluzione è quindi inviare sum in 2 messaggi da 4 byte, ricomporre il numero su server (ovviamente nell'ordine giusto)
      e poi fare l'unpack.

      La seconda soluzione era quella di convertire tutto in una stringa, insieme al nome file, per poi inviarla al server
      per farla stampare. Secondo me questa cosa va contro quello che mi è stato richiesto dall'esercizio.

*/