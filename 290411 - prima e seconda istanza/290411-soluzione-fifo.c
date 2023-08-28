#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <unistd.h>

/* pathname della FIFO utilizzata per l'interazione tra la prima e la seconda
 * istanza del programma */
char *nomefifo =
    "/tmp/fifo.compito"; /* in /tmp oppure nel direttorio corrente */

int main() {
  int cas, n = 0;
  int fifo_server, fifo_client, nread;

  /* Verifica dell'esistenza di un'istanza attiva che ha gia' creato la FIFO */
  if ((fifo_client = open(nomefifo, O_WRONLY)) < 0) {
    /* La FIFO non c'e' ancora, quindi non c'e' un'istanza attiva,
       questa esecuzione e' la prima istanza e deve creare la FIFO */

    printf("Processo %d - non riesco ad aprire la FIFO, quindi questa "
           "esecuzione e' la prima istanza e crea la FIFO\n",
           getpid());

    if (mkfifo(nomefifo, 0622) < 0) {
      perror("Errore nel creare la fifo: ");
      exit(1);
    }

    /* La FIFO appena creata va aperta prima dell'utilizzo */
    fifo_server = open(nomefifo, O_RDONLY);
    /* N.B. La open e' bloccante e ritorna solo quando una seconda istanza del
       programma fara' la corrispondente open in sola scrittura - vedi sopra */
    if (fifo_server < 0) {
      perror("Errore nell'aprire la fifo in sola lettura: ");
      exit(2);
    }

    nread = read(fifo_server, &n, sizeof(int));

    printf("Processo %d (prima istanza) - il numero casuale ricevuto e' %d,  "
           "elevato al quadrato vale %d\n",
           getpid(), n, n * n);
    close(fifo_server);

    unlink(nomefifo);

    /* Rimuovendo la FIFO dopo aver ricevuto il numero casuale, una successiva
     * esecuzione agira' come prima istanza */
    printf("Processo %d (prima istanza) - cancellata la FIFO - termino\n",
           getpid());
    exit(0);
  } else {
    /* la FIFO e' stata aperta con successo quindi c'è, ovvero c'è già una prima
     * istanza del programma in esecuzione */

    /*  Questa esecuzione e' quindi la seconda istanza : deve creare il numero
       casuale e inviarlo attraverso la FIFO alla prima istanza */

    printf("Processo %d - aperto con successo la FIFO - questa esecuzione e' "
           "la seconda istanza\n",
           getpid());

    srand(getpid()); /* inizializzazione del generatore di numeri casuali
                        utilizzando il PID del processo com seme */

    cas = 1 + rand() % 10; /*numero casuale da 1 a 10*/

    nread = write(fifo_client, &cas, sizeof(int));

    printf("Processo %d (seconda istanza) inviato %d alla prima istanza "
           "attraverso la FIFO - termino\n",
           getpid(), cas);

    close(fifo_client);
    exit(0);
  }
}
