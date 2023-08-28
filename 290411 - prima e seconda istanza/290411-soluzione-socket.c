#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define DEFAULTPORT                                                            \
  6328 /*  Porta che sara' utilizzata dalla prima istanza come server  */

int main(int argc, char *argv[]) {
  int sock, length;
  struct sockaddr_in server;
  int s, msgsock, rval;
  struct hostent *hp, *gesthostbyname();
  int myport, n, on, ret;

  myport = DEFAULTPORT;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("errore creazione stream socket");
    exit(1);
  }

  server.sin_port = htons(myport);

  server.sin_family = AF_INET;
  hp = gethostbyname("localhost");

  if (hp == 0) {
    fprintf(stderr, "%s: server sconosciuto", argv[1]);
    exit(2);
  }

  memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);

  /* La porta e' sulla linea di comando */
  server.sin_port = htons(DEFAULTPORT);

  /* Verifica dell'esistenza di un'istanza attiva come server sulla porta
   * DEFAULTPORT  */
  /* Tenta di realizzare la connessione con l'istanza preesistente */

  if (connect(sock, (struct sockaddr *)&server, sizeof server) < 0) {
    if (errno == ECONNREFUSED) {
      /* Non c'e' un server attivo sulla porta, quindi non c'e' un'istanza
         attiva, questa esecuzione e' la prima istanza e deve diventare server
         sulla porta DEFAULTPORT  */

      printf("Processo %d - non riesco a connettermi al server, quindi questa "
             "esecuzione e' la prima istanza e diventa server sulla porta %d\n",
             getpid(), DEFAULTPORT);

      /* Abilita il riutilizzo dell'indirizzo (utile se si vuole rilanciare il
         programma senza dover attendere il TIME_OUT della socket, tempo
         necessario per il protocollo TCP a chiudere effettivamente la socket
         dopo una close */
      on = 1;
      ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
      if (ret < 0)
        perror("Errore di setsockopt su stream socket: ");

      if (bind(sock, (struct sockaddr *)&server, sizeof server) < 0) {
        perror("bind su stream socket");
        exit(3);
      }
      length = sizeof server;

      if (getsockname(sock, (struct sockaddr *)&server, &length) < 0) {
        perror("Getsockname");
        exit(4);
      }

      printf("Porta utilizzata  = #%d\n", ntohs(server.sin_port));

      if (listen(sock, 2) < 0) {
        perror("listen");
        exit(5);
      }

      if ((msgsock = accept(sock, (struct sockaddr *)NULL, (int *)NULL)) < 0) {
        perror("Accept");
        exit(6);
      } else {
        read(msgsock, &n, sizeof(int));
        close(sock);
        close(msgsock);

        printf("Processo %d (prima istanza) - il numero casuale ricevuto e' "
               "%d,  elevato al quadrato vale %d\n",
               getpid(), n, n * n);
        printf("Processo %d (prima istanza) - termino\n", getpid());
        exit(0);
      }

    } else {
      perror("Errore di connessione su stream socket (diverso da ECONNREFUSED");
      exit(8);
    }
  }

  printf("Processo %d - connesso con successo al server - questa esecuzione e' "
         "la seconda istanza\n",
         getpid());

  srand(getpid()); /* inizializzazione del generatore di numeri casuali
                      utilizzando il PID del processo com seme */

  n = 1 + rand() % 10; /*numero casuale da 1 a 10*/

  write(sock, &n, sizeof(int));

  close(sock);
  printf("Processo %d (seconda istanza) inviato %d alla prima istanza "
         "attraverso la socket - termino\n",
         getpid(), n);

  exit(0);
}
