
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX(x, y) ((x > y) ? x : y)

int globalMaxMode = 0;

int globalMax = 0, got_sigusr2 = 0;

int pmax[2];

void handler(int signo) {

  if (signo == SIGUSR1) {

    globalMaxMode = !globalMaxMode;
    printf("Ricevuto SIGUSR1: globalMaxMode è %s\n",
           (globalMaxMode) ? "attivo" : "disattivo");

  } else {
    // SIGUSR2
    got_sigusr2 = 1;
    
	printf("Ricevuto SIGUSR2: globalMax sarà resettato a zero\n");

  }
}

int servizio(int connsock) {
  int p, q, nread, max;
  char mess[256] = "";
  char *inputerrmesg = "L'input deve contenere due interi\n";
  char *posparamerrmesg = "Entrambi i numeri devono essere positivi\n";
  char *hellomesg = "Immetti due interi positivi\n";

  if ((nread = read(connsock, mess, sizeof(mess))) > 0) {

    if (sscanf(mess, "%d %d", &p, &q) != 2)
      write(connsock, inputerrmesg, strlen(inputerrmesg) + 1);
    else if (p <= 0 || q <= 0)
      write(connsock, posparamerrmesg, strlen(posparamerrmesg) + 1);
    else {
      // Input ok

      if (!globalMaxMode) {
        sprintf(mess, "MAX=%d\n", MAX(p, q));
        write(connsock, mess, strlen(mess) + 1);
      } else {
        read(pmax[0], &max, sizeof(int));
        if (MAX(p, q) > max) {
          max = MAX(p, q);
        }
        write(pmax[1], &max, sizeof(int));

        sprintf(mess, "MAX=%d\n", MAX(MAX(p, q), max));
        write(connsock, mess, strlen(mess) + 1);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  int sockfd, msgsockfd, on = 1, len;
  struct sockaddr_in server, client;
  struct sigaction act;

  sigemptyset(&act.sa_mask);
  act.sa_handler = handler;
  act.sa_flags = SA_RESTART; // dato che si riceveranno segnali in accept
  sigaction(SIGUSR1, &act, NULL);
  sigaction(SIGUSR2, &act, NULL);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(1234);

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    perror("setsockopt reuseaddr:");

  if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("Errore di binding: ");
    exit(1);
  }

  listen(sockfd, 2);

  printf("Processo server PID=%d\n", getpid());

  if (pipe(pmax) < 0) {

    perror("Errore pipe");
    exit(2);
  }

  write(pmax[1], &globalMax, sizeof(int));

  do {

    msgsockfd = accept(sockfd, (struct sockaddr *)&client, (socklen_t *)&len);
    if (msgsockfd < 0) {
      perror("Errore nella accept: ");
      exit(3);
    }
    // meglio qui le eventuali operazioni dopo aver ricevuto un SIGUSR2 per
    // evitare il rischio di deadlock if (got_sigusr2) { read()
    // ;globalmaxmode=0; write
    if (got_sigusr2) {
		printf("Per effetto del SIGUSR2 globalMax è resettato a zero\n");
		read(pmax[0], &globalMax, sizeof(int));
		globalMax = 0;
		write(pmax[1], &globalMax, sizeof(int));
		got_sigusr2 = 0;
	}

    if (fork() == 0) // creazione del figlio che gestira' la connessione attuale
    {
      // FIGLIO GESTORE DELLA CONNESSIONE
      close(sockfd);
      servizio(msgsockfd);
      close(msgsockfd);
      exit(0);
    } else
      close(msgsockfd);
  } while (1);
}
