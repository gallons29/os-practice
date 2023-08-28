/********************************************************************************************

Si realizzi il seguente sistema di processi in ambiente Unix :
- il processo padre PP crea un processo Ps che inizialmente si pone in attesa di
un segnale SIGUSR1;
- il processo padre PP crea N (con 2 <= N <= 6, derivato dall'unico argomento di
invocazione del programma) processi Pi, a ciascuno dei quali viene associato il
numero NPi, ovvero l'i-esimo numero primo nella lista {2; 3; 5; 7; 11; 13};
- in seguito, ogni 2 secondi Pp genera un numero casuale Ci (con 0 <= Ci <=
10000) e lo rende disponibile a ciascun processo Pi;
- ogni processo Pi riceve Ci e nel caso esso sia multiplo di NPi lo comunica al
processo Ps;
- dopo aver inviato 10 numeri Ci, Pp invia un segnale SIGUSR1 a Ps che inizia
quindi a visualizzare i numeri ricevuti dai processi Pi;
- dopo aver ricevuto la notifica di un segnale SIGUSR1 i processi Pi devono
sospendere l'invio di messaggi a Ps; alla notifica di un segnale SIGUSR2 essi
devono riprendere il comportamento originario;
- dopo aver inviato 20 numeri Ci, Pp termina tutti i processi e termina
anch'esso.

N.B.: Ogni evento significativo per un processo deve essere descritto con un
messaggio sul terminale.


Si richiede l'utilizzo della gestione affidabile dei segnali.

********************************************************************************************/

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NMIN 2
#define NMAX 6

#define TRUE (1 == 1)
#define FALSE (1 == 0)

typedef struct _mesg {
  int id;
  int value;
} MESG;

int transmission_enabled = TRUE;
sigset_t zeromask;

// Gestore dei segnali

// Ps
void sigusr1_handler() {
  printf("Processo Ps : ho ricevuto un SIGUSR1\n", getpid());
}

// Pi
void handler(int signo) {
  printf("Processo Pi (PID %d) : ho ricevuto un segnale %s\n", getpid(),
         (signo == SIGUSR1) ? "SIGUSR1" : "SIGUSR2");

  if (signo == SIGUSR1) {
    transmission_enabled = FALSE;
    printf("Processo Pi (PID %d) : disabilito la trasmissione verso Ps\n",
           getpid());
  } else {
    if (signo == SIGUSR2) {
      transmission_enabled = TRUE;
      printf("Processo Pi (PID %d) : riabilito la trasmissione verso Ps\n",
             getpid());
    }
  }
}

// Ps
void body_ps(int rpfd) {
  MESG mesg;

  printf("Processo Ps (PID %d): attendo SIGUSR1 \n", getpid());
  sigsuspend(&zeromask);

  printf("Processo Ps (PID %d) : ricevuto SIGUSR1, procedo \n", getpid());

  while (1) {
    read(rpfd, &mesg, sizeof(mesg));
    printf("Processo Ps %d : ricevuto %d da P%d\n", getpid(), mesg.value,
           mesg.id);
  }
}

// Pi
void body_pi(int id, int np, int wpfd, int rpfd) {
  int num;
  MESG mesg;

  printf("Processo P%d (PID %d): inviero' multipli di %d a Ps\n", id, getpid(),
         np);
  while (1) {

    read(rpfd, &num, sizeof(num));
    printf("Processo P%d (PID %d): ho ricevuto %d da Pp\n", id, getpid(), num);

    if ((num % np) == 0) // Multiplo di np
    {
      if (transmission_enabled) {
        mesg.value = num;
        mesg.id = id;
        write(wpfd, &mesg, sizeof(mesg));

        printf("Processo P%d (PID %d): invio %d (e' multiplo di %d) a Ps\n", id,
               getpid(), num, np);
      } else
        printf("Processo P%d (PID %d): non invio %d (trasmissione e' "
               "disabilitata) a Ps\n",
               id, getpid(), num);
    } else
      printf(
          "Processo P%d (PID %d): non invio %d (non e' multiplo di %d) a Ps\n",
          id, getpid(), num, np);
  }
}

int main(int argc, char *argv[]) {

  struct sigaction sig;
  sigset_t sigmask;
  int ps_pid, tabpid[NMAX];
  int i, n, num, ni;
  int pipe_pi[NMAX][2]; // Un vettore di vettori (matrice) di file descriptor
  int pipe_ps[2];
  int np[6] = {2, 3, 5, 7, 11, 13};

  if (argc < 2 || atoi(argv[1]) < NMIN || atoi(argv[1]) > NMAX) {
    printf("Errore negli argomenti.\n");
    printf("Uso: programma <N>\n");
    printf("N deve essere compreso tra 2 e 6\n");
    exit(1);
  }

  // Recupero N
  n = atoi(argv[1]);

  sigemptyset(&zeromask);

  // installo il gestore del segnale SIGUSR1 in modo che il figlio Ps lo erediti
  sig.sa_handler = sigusr1_handler;
  sigemptyset(&sig.sa_mask);
  sig.sa_flags = 0; // non c'e' bisogno di flag particolari
  sigaction(SIGUSR1, &sig, NULL);

  /* Blocco del segnale SIGUSR1  per Ps che lo dovra' attendere */
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);

  sigprocmask(SIG_BLOCK, &sigmask, NULL);

  // Pipe private per ogni Pi per avere un singolo scrittore (Pp), un singolo
  // lettore (Pi) (sarebbe stato possibile ma pi� complesso utilizzare un'unica
  // pipe)
  for (i = 0; i < n; i++)
    if (pipe(pipe_pi[i]) < 0) {
      perror("creazione pipe private per i Pi:");
      exit(2);
    }

  // Pipe da Pi a Ps : scrittori multipli (Pi), singolo lettore (Ps)
  if (pipe(pipe_ps) < 0) {
    perror("creazione pipe Pi->Ps:");
    exit(3);
  }

  // Creazione processo Ps (eredita la gestione segnali del padre Pp)
  if ((ps_pid = fork()) < 0) {
    perror("creazione processo Ps:");
    exit(4);
  } else if (ps_pid == 0) {

    // Body figlio Ps
    body_ps(pipe_ps[0]);
    exit(0);
  }

  // Gestione segnali da far ereditare ai figli Pi
  sig.sa_handler = handler;
  sigemptyset(&sig.sa_mask);
  sig.sa_flags = SA_RESTART; // Per evitare che l'arrivo del segnale possa
                             // interrompere la read sulla pipe
  sigaction(SIGUSR1, &sig, NULL);
  sigaction(SIGUSR2, &sig, NULL);

  // I processi Pi devono poter ricevere SIGUSR1
  // che e' ancora bloccato per la sigprocmask precedente:
  // va sbloccato prima di creare i Pi
  sigprocmask(SIG_UNBLOCK, &sigmask, NULL);

  // Creo i processi Pi
  for (i = 0; i < n; i++) {
    if ((tabpid[i] = fork()) < 0) {
      perror("fork:");
      exit(5);
    }
    if (tabpid[i] == 0) {
      // Codice eseguito dai figli Pi che leggeranno da pipe_pi[i][0] e
      // scriveranno su pipe_ps[1]
      printf("Processo P%d (PID %d): in esecuzione\n", i, getpid());
      body_pi(i, np[i], pipe_ps[1], pipe_pi[i][0]);
      exit(0);
    }
  }

  // Codice Pp

  srand(getpid());

  for (ni = 1; ni <= 20; ni++) {

    num = rand() % 10001;
    printf("%d) Processo Pp invio %d ai processi Pi\n", ni, num);

    // Invio a ciascun Pi sulla sua pipe privata
    for (i = 0; i < n; i++)
      write(pipe_pi[i][1], &num, sizeof(int));

    if (ni == 10) {
      kill(ps_pid, SIGUSR1);
      printf("Processo Pp : dopo 10 numeri, ho inviato SIGUSR1 a Ps\n");
    }

    sleep(2);
  }

  printf("Processo Pp : termino tutti gli altri processi\n");
  kill(ps_pid, SIGTERM);

  for (i = 0; i < n; i++)
    kill(tabpid[i], SIGTERM);

  printf("Processo Pp : termino\n");
  exit(0);
}
