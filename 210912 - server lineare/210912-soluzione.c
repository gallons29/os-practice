#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/select.h>

#define PORT 2109

int q = 0; // serve una variabile globale che possa essere vista anche
           // dall'handler dei segnali

void handler(int signo) {
  printf("Processo %d: ricevuto il segnale %s: ", getpid(),
         (signo == SIGUSR1) ? "SIGUSR1" : "SIGUSR2");

  if (signo == SIGUSR1) {
    if (q < 100)
      q++;
  }

  if (signo == SIGUSR2) {
    if (q > 0)
      q--;
  }

  printf("q ora vale %d\n", q);
}

int main(int argc, char *argv[]) {
  int sock, msgsock, lenght, piped[2], num, pid, ris;
  int m, current_m; // M inizialmente vale 1
  int available;
  struct sockaddr_in server, client;
  char line_in[256], line_out[256];
  struct sigaction act;
  fd_set readfds;
  struct timeval timeout;
  int on = 1;

  if (argc != 2) {
    printf("Errore. Uso programma: %s <q>\n", argv[0]);
    exit(1);
  }

  m = 1; // valore iniziale

  q = atoi(argv[1]); // ottengo il parametro q da input

  if ((q < 0) || (q > 100)) {
    printf("Errore. L'argomento 'q' deve essere compreso tra 0 e 100!\n");
    exit(2);
  }

  if (pipe(piped) < 0) {
    perror("Creazione pipe");
    exit(3);
  }

  // gestione affidabile dei segnali
  sigemptyset(&act.sa_mask); // azzero la mask
  act.sa_handler = handler;
  act.sa_flags = SA_RESTART;

  if ((sigaction(SIGUSR1, &act, NULL)) <
      0) // applico la sigaction act al SIGUSR1
  {
    perror("Errore sigaction SIGUSR1");
    exit(4);
  }

  if ((sigaction(SIGUSR2, &act, NULL)) <
      0) // applico la sigaction act al SIGUSR2
  {
    perror("Errore sigaction SIGUSR1");
    exit(1);
  }

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) // creo la socket
  {
    perror("Creazione stream socket");
    exit(5);
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY; // Imposto i parametri della socket
  server.sin_port = htons(PORT);

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    perror("setsockopt");
  }

  if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("Binding stream socket"); ////////////
    exit(6);
  }

  lenght = sizeof(server);

  if (getsockname(sock, (struct sockaddr *)&server, (socklen_t *)&lenght) <
      0) // leggo l'indirizzo effettivo della socket
  {
    perror("Getting socket name"); ///////
    exit(7);
  }

  printf("Porta della socket # %d\n", ntohs(server.sin_port));

  listen(sock, 4);
  printf("In attesa di connessioni...\n");

  do {
    msgsock = accept(sock, (struct sockaddr *)&client,
                     (socklen_t *)&lenght); // attende un client
    printf("Connessione effettuata con successo con %s\n",
           inet_ntoa(client.sin_addr));

    // Verifica della disponibilità di un nuovo valore di M


    // Versione basata ioctl FIONREAD con cui si può
    // gestire anche la presenza di più valori di M
    // nella pipe nel caso più client mandino
    // contemporaneamente valori multipli di 100
    current_m = m;
    do {
      // Questa ioctl ritorna in available
      // il numero di byte disponibili
      // in lettura dalla pipe
      ioctl(piped[0], FIONREAD, &available);
      if (available > 0)
        read(piped[0], &m, sizeof m);
    } while (available > 0);
    // Verrà utilizzato l'unico o l'ultimo valore
    // di m presente nella pipe

    if (m != current_m)
      printf("Processo server: nuovo valore M = %d\n", m);

    /*
    
    
    // versione alternativa utilizzando select
        
    FD_ZERO(&readfds);
    FD_SET(piped[0], &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0; // la select deve essere non bloccante
        
        
    if (select(piped[0]+1,&readfds,NULL,NULL,&timeout) >0) {
    // E' inutile verificare con FD_ISSET qual e' il descrittore
    // pronto : non puo' essere altro che piped[0]

    read(piped[0],&m,sizeof m);    // in realtà potrebbero esserci
    // più valori e andrebbe recuperato l'ultimo 
    printf("Processo server: nuovo valore M = %d\n",m);
    }
    */
    
    
    // un'ulteriori alternativa poteva utilizzare una comunicazione implicita
    // con exit(m) nel figlio e  waitpid(-1, &status, WNOHANG); nel padre per
    // recuperare senza bloccarsi il valore di m

    if ((pid = fork()) == 0) {
      // processo figlio dedicato alla nuova connessione
      close(sock);

      memset(line_in, '\0', sizeof line_in);
      memset(line_out, '\0', sizeof line_out);

      if (read(msgsock, line_in, sizeof(line_in)) < 0) {
        perror("Errore read");
        exit(8);
      }

      // Estrazione dell'intero dal messaggio
      sscanf(line_in, "%d", &num);
      printf("Numero ricevuto = %d\n", num);

      // controllo sul dato ricevuto dal cliente
      if ((num % 100) == 0 && num > 0) // se il dato cliente è multiplo di 100
      {
        printf("Il numero %d e' multiplo di 100 : modifico M\n", num);

        m = num / 100; // calcolo il nuovo valore di M

        printf("Il nuovo valore di M e' %d : lo comunico al padre\n", m);

        // Comunico il nuovo valore di M al processo padre
        if (write(piped[1], &m, sizeof m) < 0) {
          // in modo che i nuovi figli avranno il valore aggiornato
          perror("Errore pipe in scrittura");
          exit(9);
        }
      }

      printf("I dati sono: M=%d   num=%d   q=%d\n", m, num, q);
      ris = (m * num) + q;
      printf("Il risultato e' %d: lo invio al client.\n", ris);

      // creo una stringa di risposta da comunicare al client
      sprintf(line_out, "Il risultato e' %d (M=%d   num=%d   q=%d)\n", ris, m,
              num, q);

      // comunico la risposta al client
      if (write(msgsock, line_out, strlen(line_out) + 1) < 0) {
        perror("Errore write");
        exit(10);
      }
      close(msgsock);
      exit(0);
    }

    // processo padre

    close(msgsock);
  } while (1);
}
