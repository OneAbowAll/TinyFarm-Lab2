# Tiny Farm - Scelte implementative

## Collector (Server)
La gestione dei cliente nel server viene gestita da più thread, uno per ogni client. In ogni thread si aspetta prima un messaggio in cui viene segnalato l'azione che il client sta cercando di compiere: 
``` 
    "?" => Il client sta inviando il valore calcolato dal file e il nome del file.

    "!" => Il client vuole chiudere il server.
```
Se il server riceve una richiesta di chiusura verrà scatenato un SIGINT all'interno del processo, questo andarà a chiudere il socket del server che si occupa di ricevere richieste di connesione (socket.accept()).
Questo socket è stato salvato al momento della sua creazione in una variabile globale.


## Produttore / Consumatori
Per gesitire il meccanismo produttore/consumatore sono andato a definire una struct che va a servire a tutti i consumatori(Workers) gli oggetti necessari per il corretto funzionamento:
- 2 semafori
- 1 mutex
- buffer dei file
- indice del buffer

## Gestione segnale SIGINT
Per la gestione dei segnali mi vado a creare un thread specializzato che rimane in ascolto per un SIGINT dentro un ciclo while infinito, questo uscirà del ciclo solo dopo aver ricevuto il segnale desiderato, questo lo porterà alla terminazione.

Prima di terminare il thread va a modificare una variabile globale a 1: ``` volatile sig_atomic_t stop; ```.<br>
Questa variabile viene usata dal MasterWorker per capire se deve smettere di inviare file prematuramente.

Se il segnale SIGINT non è mai stato inviato il MasterWorker farà una _kill_ per far uscire il thread dal while e per poter fare il join del thread.


## Informazioni Utili
- Per comodità alcune funzioni utili alla connessione e comunicazione con __collector__ sono state spostate dentro xsocket.c.

- Ho lasciato dentro farm.c dei "flag" che se settati a 1 permettono di avere più informazioni riguardo a quello che sta succedendo tra i vari thread, sono utili nel caso il programma non funzioni ma si raccomanda di tenerle a 0.

- Per inviare il numero long calcolato dai vari file ho dovuto dividere il numero in 2 parti e inviarle separatamente, nel file xsocket.c è possibile vedere come ho implementato la cosa. Il motivo di questa scelta è perchè il numero calcolato in z0.dat è rappresentabile solo con 8 bytes mentre la funzione ```htonl()``` converte solo numeri rappresentabili al massimo con 4 byte, quindi prima invio i primi 4 bytes e poi il resto.

- Prima di fare il return del main ho inserito uno sleep di 1 secondo, server soltanto a mostrare meglio l'output del programma quando lo andiamo ad eseguire su una sola linea di comando come richiesto dalle istruzioni.