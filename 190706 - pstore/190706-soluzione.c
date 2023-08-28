/*TESTO

Si realizzi in ambiente Unix/C il SERVER della seguente interazione tra
processi:

- il sistema consiste di 3 tipi di processi: server Ps, un processo Pstore e i
clienti Pci;
- per la comunicazione tra Ps e i processi clienti Pci vengono utilizzate socket
Stream;
- il server Ps offre un servizio concorrente (un figlio per ogni connessione)
alla porta 876 oppure alla porta 8765 (giustificare)
- inizialmente il server Ps attiva un processo figlio persistente Pstore che
visualizza il proprio PID e gestisce un vettore si 10 elementi interi aggiornato
sulla base delle richieste dei clienti
- i client inviano stringhe del tipo

  "STORE indirizzo valore_intero" con 0<=indirizzo<=9. Ad esempio "STORE 0 11".
La richiesta sovrascrive il valore eventualmente gia' memorizzato all'indirizzo
specificato.

- alla ricezione del segnale SIGUSR1 il processo Pstore deve visualizzare il
contento del vettore;
- alla ricezione del segnale SIGUSR2 il processo Pstore deve azzerare tutti gli
elementi del vettore;

Deve essere usata la gestione affidabile dei segnali.

Come generico client Ps si suggerisce l'utilizzo del programma telnet, invocato
come "telnet localhost numeroportaserver" (es. "telnet localhost 8765"). Una
volta connessi al sevrer scrivere, ad esempio, STORE 8 22 [INVIO] nel terminale
del telnet e inviare un SIGUSR1 al processo Pstore per verficare il
funzionamento del server.

Si siggerisce l'utilizzo delle funzioni C sscanf per estrarre dal messaggio
l'indirizzo del messaggio e il valore da assegnare, e strcmp per verificare la
presenza della parola chiave STORE
*/
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8765 // porta non privilegiata

int storeVet[10];
int sock, msgsock;

// Funzione di gestione segnali. Se ne potevano fare anche 3 diverse
void handler(int signo) {
  static int i = 0;

  switch (signo) // che segnale ho riceuvuto?
  {
  case SIGUSR1: {
    printf("\nVisualizzo i valori del vettore: ");

    for (i = 0; i < 10; i++)
      printf("%d ", storeVet[i]);

    printf("\n");
    break;
  }
  case SIGUSR2: {
    printf("\nAzzero i valori del vettore\n");
    memset(storeVet, 0, sizeof(storeVet));
    break;
  }
  case SIGINT: {
    printf("\nProcesso %d esce\n", getpid());

    close(sock);
    close(msgsock);
    exit(0);
  }
  }
}

int main() {
  struct // ho pensato di usare una struttura per leggere la posizione e il
         // numero da inserire
  {
    int pos;
    int num;
  } storeRec;

  char buffer[256] = "";
  char comando[256] = "";
  int pidStore;

  int p[2]; // la pipe

  struct sigaction sig, sig1, sig2;

  struct sockaddr_in server, client;
  int lenght, rval;

  if (pipe(p) < 0) // creo la pipe, sara' comune per tutti i processi
  {
    perror("pipe");
    exit(1);
  }

  sig2.sa_handler = handler; // installo il gestore del segnale di uscita per
                             // tutti i processi. Non obbligatorio
  sigemptyset(&sig2.sa_mask);
  sig2.sa_flags = SA_RESTART;

  sigaction(SIGINT, &sig2, NULL);

  if ((pidStore = fork()) < 0) // creo il figlio Pstore, che sara' permanente
                               // fino alla fine del programma
  {
    perror("fork Pstore");
    exit(1);
  } else if (pidStore == 0) // sono il figlio(Pstore)?
  {
    // PSTORE
    printf("Sono il Pstore con pid %d\n", getpid());

    close(p[1]); // chiudo il descrittore discrittura della pipe, non mi serve

    sig.sa_handler = handler; // installo il gestore del segnale SIGUSR1
    sigemptyset(&sig.sa_mask);
    sigaddset(&sig.sa_mask,
              SIGUSR2); // blocco SIGUSER2 durante la gestione di SIGUSR1
    sig.sa_flags = SA_RESTART;

    sig1.sa_handler = handler; // installo il gestore del segnale SIGUSR2
    sigemptyset(&sig1.sa_mask);
    sigaddset(&sig1.sa_mask,
              SIGUSR1); // blocco SIGUSER2 durante la gestione di SIGUSR2
    sig1.sa_flags = SA_RESTART;

    sigaction(SIGUSR1, &sig, NULL);
    sigaction(SIGUSR2, &sig1, NULL);

    do // ciclo infinito di lettura dalla pipe
    {
      if (read(p[0], &storeRec, sizeof(storeRec)) <
          0) // leggo il dato che contiene posizione e nuovo numero
        perror("read pipe");

      printf("Pstore: ricevuto %d %d\n", storeRec.pos, storeRec.num);
      storeVet[storeRec.pos] = storeRec.num; // setto in nuovo numero
    } while (1);
  } else // altrimenti sono il padre (server)
  {
    // PADRE
    printf("Sono il padre con pid %d\n", getpid());

    sock = socket(AF_INET, SOCK_STREAM, 0); // creo la socket
    if (sock < 0) {
      perror("creazione stream socket");
      kill(pidStore, SIGINT);
      exit(2);
    }

    // preparo la struttura sockaddr_in per settare i parametri della socket
    server.sin_family = AF_INET;         // internet protocol
    server.sin_addr.s_addr = INADDR_ANY; // ascolto da tutte le interfacce
    server.sin_port = htons(PORT);       // e dalla porta specificata
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
      perror("binding stream socket");
      kill(pidStore, SIGINT);
      exit(3);
    }

    lenght = sizeof(server);
    if (getsockname(sock, (struct sockaddr *)&server, (socklen_t *)&lenght) <
        0) // leggo il nome della socket
    {
      perror("getting socket name");
      kill(pidStore, SIGINT);
      exit(3);
    }

    printf("Socket port # %d\n", ntohs(server.sin_port));

    listen(sock, 5); // massimo di 5 connessioni in attesa

    do // questo e' il ciclo principale del server: attende connessioni, e crea
       // un figlio per ogni una
    {
      msgsock = accept(sock, (struct sockaddr *)&client,
                       (socklen_t *)&lenght); // attendo connessioni...

      if (msgsock == -1) {
        perror("accept");
        exit(4);
      }

      if (fork() == 0) // genero il figlio che gestira' la connessione attuale
      {
        // FIGLIO GESTORE DELLA CONNESSIONE
        close(sock);
        close(p[0]);

        do {
          if ((rval = read(msgsock, buffer, 256)) < 0) // leggo il messaggio
          {
            perror("reading message");
            exit(5);
          }

          // nr=sscanf()   == 3  da controllare
          sscanf(buffer, "%s %d %d", comando, &(storeRec.pos),
                 &(storeRec.num)); // estraggo comando, posizione nel vettore e
                                   // nuovo valore
          printf("Figlio socket: ricevuto %s %d %d\n", comando, storeRec.pos,
                 storeRec.num);

          if (!strncmp(comando, "STORE", 5)) // se il comando era effettivamente
                                             // STORE, agisco di conseguenza
          {
            if (write(p[1], &storeRec, sizeof(storeRec)) < 0) {
              perror("write pipe");
              exit(6);
            }
          }
          while (1)
            ;
        }
        else close(msgsock);
      }
      while (1)
        ;
    }

    return 0;
  }
