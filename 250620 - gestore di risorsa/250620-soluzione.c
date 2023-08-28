#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT 10020
#define QUEUE_SIZE 100

void ASSERT(int condition, const char *message) {
  if (condition) {
    fprintf(stderr, "%s", message);
    exit(1);
  }
}

struct res {
  int available;
  int code;
};

struct MESG {
  int pid;
  char buf[64];
};

struct res R;
int run = 1;

void sig2_handler(int sig) { printf("Authorized for reading\n"); }

void sig1_handler(int sig) {
  run = !run;
  printf("Service is %s\n", (run) ? "active" : "inactive");
}

int main(int argc, char **argv) {
  char buf[64];
  struct sigaction act, act2;

  R.available = 1;
  R.code = 0;
  struct sockaddr_in serv;
  serv.sin_family = AF_INET;
  serv.sin_port = htons(PORT);
  serv.sin_addr.s_addr = INADDR_ANY;
  printf("Setting up server...PID %d\n", getpid());
  int out[2];
  int in[2];
  ASSERT(pipe(out) < 0, "Invalid pipe\n");
  ASSERT(pipe(in) < 0, "Invalid pipe\n");
  int handler = fork();
  ASSERT(handler < 0, "Invalid fork\n");

  if (!handler) // Processo gestore della risorsa
  {
    // child handler
    struct MESG m;
    struct MESG queue[QUEUE_SIZE];
    int end_queue, curpos;
    srand(time(NULL));
    curpos = 0;
    end_queue = 0;
    char buf[64];
    int received_id = 0;
    printf("Resource handler started\n");
    while (1) {
      printf("R status: %d\n", R.available);
      received_id = 0;
      ASSERT(read(in[0], &m, sizeof m) < 0, "Invalid read1");
      ASSERT(sscanf(m.buf, "%s %d", buf, &received_id) <= 0, "Error scanf\n");
      printf("Child handler received %s\n", m.buf);
      if (strcmp(buf, "P") == 0) {
        printf("Found P... client requesting resource\n");
        if (R.available == 1) {
          printf("R is available and will be issued\n");
          R.available = 0;
          R.code = rand() % 1000000 + 1;
          sprintf(m.buf, "K %d", R.code);
          printf("Buf: %s\n", m.buf);
          ASSERT(write(out[1], &m, sizeof m) < 0, "Invalid write1\n");
          kill(m.pid, SIGUSR2);

        } else {
          printf("R is unavailable he'll have to wait\n");
          queue[end_queue++] = m;
          sprintf(m.buf, "W");
          ASSERT(write(out[1], &m, sizeof m) < 0, "Invalid write2\n");
          kill(m.pid, SIGUSR2);
        }
      } else {
        printf("Received R\n");
        if (R.available == 1) {
          printf("R already available, no R needed\n");
          sprintf(m.buf, "E");
          ASSERT(write(out[1], &m, sizeof m) < 0, "Invalid write3\n");
          kill(m.pid, SIGUSR2);
        } else {
          if (received_id == R.code) {
            if (curpos == end_queue) {
              printf("Correct code and no queue-> giving resource to %d\n",
                     m.pid);
              R.available = 1;
              sprintf(m.buf, "OK");
              ASSERT(write(out[1], &m, sizeof m) < 0, "Invalid write4\n");
              kill(m.pid, SIGUSR2);
            } else {
              printf("Correct code and queue is not empty\n");
              R.code = rand() % 1000000 + 1;
              sprintf(m.buf, "%d", R.code);
              printf("Buf: %s\n", m.buf);
              ASSERT(write(out[1], &m, sizeof m) < 0, "Invalid write5\n");
              kill(queue[curpos++].pid, SIGUSR2);
              sleep(1);
              sprintf(m.buf, "OK");
              ASSERT(write(out[1], &m, sizeof m) < 0, "Invalid write6\n");
              kill(m.pid, SIGUSR2);
              printf("R given to %d\n", m.pid);
            }
          } else {
            printf("Unable to decode\n");
            sprintf(m.buf, "E");
            ASSERT(write(out[1], &m, sizeof m) < 0, "Invalid write7\n");
            kill(m.pid, SIGUSR2);
          }
        }
      }
    }

  } else {
    // Processo main server

    act2.sa_handler = sig1_handler;
    act2.sa_flags = SA_RESTART;
    sigemptyset(&act2.sa_mask);
    sigaction(SIGUSR1, &act2, NULL);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(sock < 0, "Invalid sock\n");
    printf("Socket created\n");
    int on = 1;
    ASSERT(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0,
           "Invalid optn\n");

    ASSERT(bind(sock, (struct sockaddr *)&serv, sizeof serv) < 0,
           "Invalid bind\n");
    printf("Socket bound to %d\n", PORT);
    ASSERT(listen(sock, 10) < 0, "Invalid listen\n");
    printf("Listen mode on\n");
    while (1) {
      int client = accept(sock, NULL, NULL);

      if (!run) {
        sprintf(buf, "Service not available\n");
        ASSERT(write(client, buf, strlen(buf)) < 0,
               "Invalid write to client\n");
      } else {

        int child = fork(); // Figlio dedicato al cliente che si è connesso
        ASSERT(child < 0, "Invalid fork\n");

        if (!child) {
          // server concorrente

          act.sa_handler = sig2_handler;
          act.sa_flags = SA_RESTART;
          sigemptyset(&act.sa_mask);
          sigaction(SIGUSR2, &act, NULL);

          sigset_t zero, mask;
          sigemptyset(&zero);
          sigemptyset(&mask);
          sigaddset(&mask, SIGUSR2);

          close(sock);

          int received_id = 0;
          struct MESG m;
          printf("Client accepted and served by PID: %d\n", getpid());
          while (1) {

            sigprocmask(SIG_BLOCK, &mask, NULL);
            memset(m.buf, 0, sizeof m.buf);
            int was_stopped = read(client, m.buf, sizeof m.buf);
            if (was_stopped > 0) {

              m.pid = getpid();
              ASSERT(write(in[1], &m, sizeof m) < 0, "Invalid write8\n");
              printf("Waiting for response\n");
              // suspend here

              sigsuspend(&zero);
              sigprocmask(SIG_UNBLOCK, &mask, NULL);

              ASSERT(read(out[0], &m, sizeof m) < 0, "Invalid read3\n");
              printf("Server responded with: %s\n", m.buf);
              ASSERT(sscanf(m.buf, "%s %d", buf, &received_id) <= 0,
                     "Invalid scanf\n");

              if (strcmp(buf, "W") == 0) {

                strcpy(buf, "Waiting for resource...\n");
                ASSERT(write(client, buf, strlen(buf)) < 0,
                       "Invalid write to client\n");
                // suspend here
                sigprocmask(SIG_BLOCK, &mask, NULL);
                sigsuspend(&zero);
                sigprocmask(SIG_UNBLOCK, &mask, NULL);

                ASSERT(read(out[0], &m, sizeof m) < 0, "Invalid read4\n");
                ASSERT(sscanf(m.buf, "%d", &received_id) <= 0,
                       "Invalid scanf\n");
                printf("Received %d\n", received_id);
                ASSERT(received_id == 0, "Error id\n");
                sprintf(buf, "Resource is yours: %d\n", received_id);
                ASSERT(write(client, buf, strlen(buf)) < 0,
                       "Invalid write to client\n");

              } else if (strcmp(buf, "OK") == 0) {

                strcpy(buf, "Released successfully\n");
                ASSERT(write(client, buf, strlen(buf)) < 0,
                       "Invalid write to client\n");
              } else if (strcmp(buf, "E") == 0) {

                strcpy(buf, "Error\n");
                ASSERT(write(client, buf, strlen(buf)) < 0,
                       "Invalid write to client\n");
              } else if (strcmp(buf, "K") == 0) {

                ASSERT(received_id == 0, "Error id\n");
                sprintf(buf, "Resource is yours: %d\n", received_id);
                ASSERT(write(client, buf, strlen(buf)) < 0,
                       "Invalid write to client\n");

              } else {
                ASSERT(1, "Error\n");
              }
            }
          }
        }
      }
    }
  }
}
