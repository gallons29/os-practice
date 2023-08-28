#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define TRUE (1 == 1)
#define FALSE (1 == 0)

typedef struct _mesg {
  int num;
  int value;
} MESG;

int i;
int NP[6] = {2,3,5,7,11,13};
int is_sending = TRUE;

void body_proc(int id, int rpfd, int wpfd)
{
	int c;
	MESG mesg;
	while(1)
	{
		read(rpfd, &c, sizeof(int));
		//printf("%d riceve %d\n", getpid(), c);
		if(c%NP[id]==0 && is_sending)
		{
			mesg.num = NP[id];
			mesg.value = c; 
			write(wpfd, &mesg, sizeof(mesg));
		}
	}
}

void handler_figli(int signo)
{
	if(signo == SIGUSR1)
	{
		printf("Ricevuto SIGUSR1 su Pi. Sending di Pi disabilitata\n");
		is_sending = FALSE;
	}
	if(signo == SIGUSR2)
	{
		printf("Ricevuto SIGUSR2 su Pi. Sending di Pi abilitata\n");
		is_sending = TRUE;
	}
}

void handler_ps(int signo)
{
	printf("Handler Ps, ricevuto\n");
}

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		printf("Uso: %s [2-6]\n", argv[0]);
    exit(1);
	} else if(atoi(argv[1]) < 2 || atoi(argv[1]) > 6) {
		printf("Il numero deve essere compreso tra 2 e 6\n");
    exit(1);
	}
	
	const int N = atoi(argv[1]);
	int tabpid[N];
	int c;
	
	int pid, pids;
	
	int pipe_pi[N][2];
	//Pipe Pi
	for(int j=0; j<N; j++)
	{  
		if(pipe(pipe_pi[j])<0)
		{
			perror("pipe error");
			exit(-2);
		}
	}
	
	//Pipe Ps
	int pipe_ps[2];
	if(pipe(pipe_ps)<0)
	{
		perror("pipe error");
		exit(-2);
	}
	
	sigset_t zeromask;
  struct sigaction action;
 	
 	sigemptyset(&zeromask);
  /* azzera tutti i flag della maschera sa_mask nella struttura action */
  sigemptyset(&action.sa_mask);
  action.sa_handler = handler_ps;
  action.sa_flags = 0;

  /* assegna action per la gestione di SIGUSR1 per Ps */
  if (sigaction(SIGUSR1, &action, NULL) == -1)
    perror("sigaction error");


	//Ps
	if((pids=fork())==0)
	{
		sigsuspend(&zeromask);
		MESG mesg;
		while(1)
		{
			read(pipe_ps[0], &mesg, sizeof(mesg));
			printf("Ps, ricevuto %d multiplo di %d\n", mesg.value, mesg.num);
		}
	}
	
	action.sa_handler = handler_figli;
	/* assegna action per la gestione di SIGUSR1 e SIGUSR2 */
  if (sigaction(SIGUSR1, &action, NULL) == -1)
    perror("sigaction error");
    
  if (sigaction(SIGUSR2, &action, NULL) == -1)
    perror("sigaction error");
	
	//Pi
	for(int j=0; j<N; j++)
	{
		if((tabpid[j]=fork())<0)
		{
			perror("Fork error");
			exit(-1);
		}
		else if (tabpid[j]==0){
			body_proc(j, pipe_pi[j][0], pipe_ps[1]);
		}
	}
	
	//Padre
	srand(time(NULL));
	for(i=0;i<20;i++)
	{
		if(i==1)
		{
			kill(pids, SIGUSR1);
		}
		
		
		c = rand()%10001;
		printf("c=%d\n",c);
		for(int j=0; j<N; j++)
		{
			write(pipe_pi[j][1], &c, sizeof(int));
		}
		sleep(2);
		
	
	}
 
	kill(pids, SIGKILL);
	for(int j=0; j<N; j++)
	{
		kill(tabpid[j], SIGKILL);
	}
  exit(0);
}
