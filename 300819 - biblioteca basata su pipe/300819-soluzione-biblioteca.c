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

#include <errno.h>

#define SERVERPORT 1234

#define NMAX 4
#define NMIN 10

typedef struct _libro {
  int idLibro;
  char titolo[64];
  char autore[64];
  int disponibile;
  int codice;

} libro;

int serviceAvailable = 1;
int nLibri;

// una pipe per ogni libro : la risorsa è suddivisa in sottorisorse
// vedi sotto il commento sulla maggiore concorrenza che si ottiene
int piped[10][2];

// Funzione di gestione segnali.
void handler(int signo) {

  serviceAvailable = !serviceAvailable;
  printf("SIGUSR1: service is %s\n",
         (serviceAvailable) ? "ACTIVE" : "not ACTIVE");
}

int elencaLibri(int msgsock);

int prelevaLibro(int msgsock, int idLibro);

int restituisciLibro(int msgsock, int idLibro, int codice);

int bodyService(int msgsock) {
  char buffer[256], answer[128];
  int rval, pipeID, balance, amount;
  char *hello = "Comandi disponibili : L [oppure] P idLibro [oppure] R idLibro "
                "codice [oppure] F\n";
  char *requestError =
      "Errore: comando o parametro errato oppure richiesta malformata\n";
  char *bye = "Fine della sessione\n";

  char cmd;
  int idLibro, codice;
  int nParams;

  printf("Account Service (PID %d) is %s\n", getpid(),
         (serviceAvailable) ? "ACTIVE" : "NOT ACTIVE");

  // invia un messaggio con i comandi supportati al client
  write(msgsock, hello, strlen(hello) + 1);

  do {
    if ((rval = read(msgsock, buffer, 256)) < 0) // leggo il messaggio
      perror("reading message");

    if (rval == 0)
      continue; // il cliente ha chiuso la connessione

    nParams = sscanf(buffer, "%c %d %d", &cmd, &idLibro, &codice);
    printf("sscanf nparams=%d (%c,%d,%d)\n", nParams, cmd, idLibro, codice);

    // Per i comandi P e Q l'idLibro deve essere compreso tra 0 e nLibri-1 
    if ((cmd != 'L' && cmd != 'F') && (idLibro < 0 || idLibro > nLibri - 1)) {
      write(msgsock, requestError, strlen(requestError) + 1);
      continue;
    }

    if (cmd == 'L')
      elencaLibri(msgsock);
    else if (cmd == 'P')
      prelevaLibro(msgsock, idLibro);
    else if (cmd == 'R')
      restituisciLibro(msgsock, idLibro, codice);
    else if (cmd == 'F') {
      write(msgsock, bye, strlen(bye) + 1);
      close(msgsock);
      return 0;
    } else
      write(msgsock, requestError, strlen(requestError) + 1);

  } while (rval != 0);
}

int elencaLibri(int msgsock) {

  int i;
  libro descLibro;
  char answer[256];
  char *msgElenco = "Elenco dei libri disponibili in biblioteca:\n";

  write(msgsock, msgElenco, strlen(msgElenco));

  for (i = 0; i < nLibri; i++) {

    read(piped[i][0], &descLibro, sizeof(descLibro));
    write(piped[i][1], &descLibro, sizeof(descLibro));
    if (descLibro.disponibile) {
      sprintf(answer, "Libro %d autore:%s titolo:%s\n", descLibro.idLibro,
              descLibro.titolo, descLibro.autore);
      write(msgsock, answer, strlen(answer) + 1);
    }
  }
}

int prelevaLibro(int msgsock, int idLibro) {
  libro descLibro;
  int codice;
  char answer[256];
  char *msgNonDisp = "Il libro %d non e' disponibile\n";

  read(piped[idLibro][0], &descLibro, sizeof(descLibro));

  if (descLibro.disponibile) {
    srand(getpid());
    codice = rand() % 1000000;
    descLibro.disponibile = 0;
    descLibro.codice = codice;
    sprintf(answer, "OK %d\n", codice);
    write(msgsock, answer, strlen(answer) + 1);

  } else {
    write(msgsock, msgNonDisp, strlen(msgNonDisp) + 1);
  }

  write(piped[idLibro][1], &descLibro, sizeof(descLibro));
}

int restituisciLibro(int msgsock, int idLibro, int codice) {
  libro descLibro;
  char answer[256];
  char *msgNonDisp = "Codice errato per il libro ";

  read(piped[idLibro][0], &descLibro, sizeof(descLibro));

  if (descLibro.codice == codice && !descLibro.disponibile) {
    descLibro.disponibile = 1;
    descLibro.codice = 0;
    sprintf(answer, "Il libro %d e' stato correttamente restituito\n", idLibro);
    write(msgsock, answer, strlen(answer) + 1);
  } else {
    sprintf(answer, "%s %d", msgNonDisp, idLibro);
    write(msgsock, answer, strlen(answer) + 1);
  }

  write(piped[idLibro][1], &descLibro, sizeof(descLibro));
}

int main(int argc, char *argv[]) {

  char buffer[256] = "";
  char comando[256] = "";
  char titolo[64];
  char autore[64];

  int sock, msgsock;

  int i, n, balance;
  struct sigaction sig;
  libro descLibro;

  struct sockaddr_in server, client;
  int lenght, rval;
  char *serviceUnavailable = "503 Service Unavailable\n";

  if (argc > 2) {
    fprintf(stderr, "Usage: %s [N] (%d <= N <= %d)\n", argv[0], NMIN, NMAX);
    exit(1);
  }

  if (argc == 2) {
    n = atoi(argv[1]);
    if (n < NMIN || n > NMAX) {
      fprintf(stderr, "Wrong value for N (%d): setting it to %d\n", n, NMIN);
      n = NMIN;
    }
  } else
    n = NMIN;

  nLibri = n;

  sig.sa_handler = handler; // installo il gestore del segnale
  sigemptyset(&sig.sa_mask);
  sig.sa_flags = SA_RESTART;

  sigaction(SIGUSR1, &sig, NULL);
  
  // Creazione di una pipe per ogni libro: aumento la concorrenza
  // dato che le operazioni su libri diversi possono essere svolte
  // contemporaneamente

  for (i = 0; i < n; i++) {
    if (pipe(piped[i]) < 0) {
      fprintf(stderr, "creazione pipe %i (%s)", i, strerror(errno));
      exit(2);
    }
  }

  // Inizializzazione

  for (i = 0; i < n; i++) {
    descLibro.idLibro = i;
    sprintf(titolo, "Titolo%d", i);
    strcpy(descLibro.titolo, titolo);
    sprintf(autore, "Autore%d", i);
    strcpy(descLibro.autore, autore);
    descLibro.disponibile = 1;
    descLibro.codice = 0;

    write(piped[i][1], &descLibro, sizeof(descLibro));
  }

  printf("SERVER PID %d\n", getpid());

  sock = socket(AF_INET, SOCK_STREAM, 0); // creo la socket
  if (sock < 0) {
    perror("creazione stream socket");
    exit(3);
  }

  int on = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    perror("setsockopt reuseaddr:");
  }

  // preparo la struttura sockaddr_in per settare i parametri della socket
  server.sin_family = AF_INET;         // internet protocol
  server.sin_addr.s_addr = INADDR_ANY; // ascolto da tutte le interfacce
  server.sin_port = htons(SERVERPORT); // e dalla porta specificata
  if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("binding stream socket");
    exit(4);
  }

  lenght = sizeof(server);
  if (getsockname(sock, (struct sockaddr *)&server, (socklen_t *)&lenght) <
      0) // leggo il nome della socket
  {
    perror("getting socket name");
    exit(5);
  }

  printf("Socket port # %d\n", ntohs(server.sin_port));

  listen(sock, 5); // massimo di 5 connessioni in attesa

  do // questo e' il ciclo principale del server concorrente:
     // attende connessioni  e crea un figlio per ogni nuovo cliente che si
     // connette
  {
    msgsock = accept(sock, (struct sockaddr *)&client,
                     (socklen_t *)&lenght); // attendo connessioni...

    if (msgsock == -1) {
      perror("accept");
      exit(6);
    }

    if (!serviceAvailable) { // se il servizio non è attivo è inutile creare un
                             // figlio
      write(msgsock, serviceUnavailable, strlen(serviceUnavailable) + 1);
      close(msgsock);
      continue;
    } else { // servizio attivo

      if (fork() ==
          0) { // creazione del figlio che gestira' la connessione attuale

        // FIGLIO GESTORE DELLA CONNESSIONE
        close(sock);
        bodyService(msgsock);
        close(msgsock);
        exit(0);
      } else
        close(msgsock);
    }
  } while (1);

  return 0;
}
