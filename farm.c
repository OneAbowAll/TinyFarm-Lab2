// Master Worker
#define _GNU_SOURCE
#include <unistd.h>

#include "xerrori.h"
#include "xsocket.h"

#define QUI __LINE__, __FILE__
#define MAX_NAME_LENGTH 255

#define DEBUG_THRD 0
#define DEBUG_BUFF 0

#define print_debug_buffer if(DEBUG_BUFF) fprintf
#define print_debug_thread if(DEBUG_BUFF) fprintf


typedef struct
{
  char **files;
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

volatile sig_atomic_t stop = 0;
// void logBuffer(char** buffer, int qlen, int cIndex, int pIndex, sem_t *free, sem_t *item);

int main(int argc, char *argv[])
{
  #pragma region Input Management

  int opt;
  int nthread = 4, qlen = 8, delay = 0;

  // Prendi argomenti opzionali
  while ((opt = getopt(argc, argv, "n:q:t:")) != -1)
  {
    switch (opt)
    {
    case 'n':
      nthread = max(atoi(optarg), 1);
      break;

    case 'q':
      qlen = max(atoi(optarg), 1);
      break;

    case 't':
      delay = max(atoi(optarg), 0) * 1000;
      break;

    default: /* Caso ? */
      fprintf(stdout, "Parametro opzionale errato. Uso: %s file [file...] [-n nthread] [-q qlen] [-t delay] \n", argv[0]);
      break;
    }
  }

  // controlla numero argomenti, ricorda che una volta usato getopt i parametri non opzionali (i file) sono stati messi in fondo
  if (optind >= argc)
  {
    fprintf(stdout, "Specificare almeno un file. Uso: %s file [file...] [-n nthread] [-q qlen] [-t delay] \n", argv[0]);
    return 1;
  }

  fprintf(stdout, "Parametri attuali => nthread: %d; qlen: %d; delay = %d \n", nthread, qlen, delay / 1000);

  fprintf(stdout, "File Selezionati => [");
  for (int i = optind; i < argc; i++)
    (i != argc-1) ? fprintf(stdout, "%s, ", argv[i]) : fprintf(stdout, "%s", argv[i]);
  fprintf(stdout, "]\n\n");

  #pragma endregion

  // Gestione segnali
  struct sigaction sa;
  sigaction(SIGINT, NULL, &sa);
  sa.sa_handler = &intHandler;
  sigaction(SIGINT, &sa, NULL);

  // Gestione sistema produttori consumatori
  sem_t sem_free, sem_items;
  xsem_init(&sem_free, 0, qlen, QUI); // Contatore di spazio livero
  xsem_init(&sem_items, 0, 0, QUI);   // Contatore di oggetti nel buffer

  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Assicura che i Workers non si "pestino i piedi" tra di loro

  char **files; // Buffer dei nomi file
  files = malloc(sizeof(char *) * qlen);
  for (int i = 0; i < qlen; i++)
    files[i] = malloc(sizeof(char) * MAX_NAME_LENGTH);

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
    print_debug_thread(stdout, "Creato Thread %ld. \n", t[i]);
  }

  // Inserisci nel buffer tutti i
  for (int i = optind; i < argc; i++)
  {
    if(stop) break;

    xsem_wait(&sem_free, QUI);

    strcpy(files[pIndex++ % qlen], argv[i]);
    print_debug_buffer(stdout, "Scritto nel buffer %s \n", files[(pIndex - 1) % qlen]);

    xsem_post(&sem_items, QUI);

    usleep(delay);
  }

  for (int i = 0; i < nthread; i++)
  {
    xsem_wait(&sem_free, QUI);

    strcpy(files[pIndex++ % qlen], "-1");
    print_debug_buffer(stdout, "Scritto nel buffer %s \n", files[(pIndex - 1) % qlen]);

    xsem_post(&sem_items, QUI);
  }

  for (int j = 0; j < nthread; j++)
  {
    xpthread_join(t[j], NULL, QUI);
    print_debug_thread(stdout, "Joinato il Thread %ld. \n", t[j]);
  }

  closeServer();
  freeMemory(shm);
  return 0;
}

void intHandler(int s)
{
  if (stop)
    printf("\n== Segnale SIGINT == Procedura di chiusura in corso, attendere...\n");
  else
    printf("\n== Segnale SIGINT == Procedura di chiusura iniziata...\n");

  stop = 1;
}

void closeServer()
{
  int skt = connect_to_collector();
  if(skt < 0)
    termina("Errore apertura connessione");

  writen(skt, "!", 1);

  if (close(skt) < 0)
    perror("Errore chiusura socket");
}

void freeMemory(dati d)
{
  /*Libero lo spazio allocato alle singole stringhe
          e poi libero lo spazio per l'array stesso */

  for (int i = 0; i < d.qlen; i++)
    free(d.files[i]);
  free(d.files);

  // exit(1);

  /*Onestamente non so se sia il metodo giusto per fare questa cosa
          ma per me ha senso e valgrind conferma */
}

void *worker(void *arg)
{
  dati *d = (dati *)arg;
  int skt = connect_to_collector();
  
  int f;
  char *nomeFile = "-1";
  long sum = 0;

  while (true)
  {
    //Leggi dal buffer il file
    xsem_wait(d->sem_items, QUI);
    xpthread_mutex_lock(d->mutex, QUI);

    nomeFile = d->files[*(d->cIndex) % d->qlen];
    *(d->cIndex) += 1;

    xpthread_mutex_unlock(d->mutex, QUI);
    xsem_post(d->sem_free, QUI);

    //Controlla se il Master ha finito di leggere, chiuditi se si
    if (strcmp(nomeFile, "-1") == 0)
      break;

    //Apri file se non esiste il file skippa questa iterazione e passa a quella successiva
    f = xopen(nomeFile, O_RDONLY, QUI);
    if(f<0) continue;

    sum = 0;
    int cont = 0; long n;
    ssize_t e;
    while (true)
    {
      e = read(f, &n, sizeof(long));
      if (e != sizeof(long))
        break;

      sum += n * cont;
      cont += 1;
    }

    xclose(f, QUI);
    print_debug_buffer(stdout, "Calcolcato il file %s => somma %ld. \n", nomeFile, sum);

    // Avverto il server che sto inviando dei dati
    writen(skt, "?", 1);

    // Invio primo i primi 4 bytes della somma e successivamnte i bytes rimanenti
    // Per capire il perchè di questa scelta scendere a fine file (*)
    send_long(skt, sum);

    // Invio Nome File
    writen(skt, nomeFile, MAX_NAME_LENGTH);

    fprintf(stdout, "Inviato risultato file: %s\n", nomeFile);
    usleep(d->delay);
  }

  if (close(skt) < 0)
    perror("Errore chiusura socket");

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


/*
                      ======= PROMEMORIA PER IL DAVIDE DEL FUTURO =======

  (*) Perchè ho inviato la variabile sum con due messaggi diversi?

      htonl() restituisce un unsigned int da 32 bit, tuttavia io ho bisogno di inviare 64 bit (8 byte, necessari per poter rappresentare numeri tipo 9876543210.
      Questo significa che un long in c, che è da 64 bit, non viene inviato del tutto (non so bene se il numero veniva troncato al 32-esimo bit
      o se veniva fatto arrotondato al max valore che poteva avere uint_32, testando su python mi sembra che venga arrotondato).

      Quindi quando su python provavo a chiedere per 8 byte da convertire in un long long (perchè la struct di python per un long usa solo 4 byte)
      ricevava sostanzialmente roba senza senso.

      La mia soluzione consiste nel inviare sum in 2 messaggi da 4 byte ciascuno; sul server ho quindi due scelte o aspetto 2 messaggi da 4 byte che vado
      a ricomporre o aspetto direttamente 8 byte.

      La seconda soluzione era quella di convertire tutto in una stringa, insieme al nome file, per poi inviarla al server
      per farla stampare. Secondo me questa cosa va contro quello che mi è stato richiesto dall'esercizio.

*/
