#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

typedef struct _mesg {
	int address;
	int value;
} MESG;

int store[10];

void body_pstore(int rpfd)
{
	printf("PID di Pstore: %d\n", getpid());
	MESG mesg;
	while(1)
	{
		read(rpfd, &mesg, sizeof(mesg));
		printf("Ho letto address=%d e value=%d\n", mesg.address, mesg.value);
		store[mesg.address] = mesg.value;
	}
}

void handler(int signo)
{
	if(signo == SIGUSR1)
	{
		printf("Ricevuto SIGUSR1\n");
		for(int i=0; i<10; i++)
		{
			printf("Store[%d] = %d\n", i, store[i]);
		}
	}
	if(signo == SIGUSR2)
	{
		printf("Ricevuto SIGUSR2\n");
		for(int i=0; i<10; i++)
		{
			store[i] = 0;
		}
	}
}

int main(int argc, char* argv[])
{
  int	sock,length,portno;
  struct  sockaddr_in server, client;
  int	pid,pidstore,msgsock;
  struct	hostent	*hp,*gethostbyname();
  
  char comando[256];
  int indirizzo, valore;
  char buffer[256],answer[256];
  char *hellomsg = "Immettere un comando: STORE indirizzo valore_intero\n";
  
  int pipe_store[2];
  MESG mesg;

	sigset_t zeromask;
	struct sigaction action;
	sigemptyset(&zeromask);
	sigemptyset(&action.sa_mask);
	action.sa_handler = handler;
	action.sa_flags = SA_RESTART;
	if(sigaction(SIGUSR1, &action, NULL) == -1)
	{
		perror("sigaction error");
		exit(7);
	}
		if(sigaction(SIGUSR2, &action, NULL) == -1)
	{
		perror("sigaction error");
		exit(8);
	}

	
	/* Pipe store */
	if(pipe(pipe_store)<0)
	{
		perror("Errore pipe");
		exit(5);
	}
	
	/* Crea Pstore */
	if((pidstore=fork()) < 0)
	{
		perror("errore fork");
		exit(-1);
	} else if (pidstore == 0)
	{
		body_pstore(pipe_store[0]);
	}

  /* Crea la  socket STREAM */
  sock=	socket(AF_INET,SOCK_STREAM,0);
  if(sock<0)
  {	
    perror("opening stream socket");
    exit(2);
  }
  
  portno = 8765;
  
  /* Utilizzo della wildcard per accettare le connessioni ricevute da ogni interfaccia di rete del sistema */
  server.sin_family = 	AF_INET;
  server.sin_addr.s_addr= INADDR_ANY;
  server.sin_port = htons(portno);
  if (bind(sock,(struct sockaddr *)&server,sizeof server)<0)
  {
    perror("binding stream socket");
    exit(3);
  }
  
  
  length= sizeof(server);
  if(getsockname(sock,(struct sockaddr *)&server,&length)<0)
  {
    perror("getting socket name");
    exit(4);
  }
  
  printf("Socket port #%d\n",ntohs(server.sin_port));
  
  
  /* Pronto ad accettare connessioni */
  
  listen(sock,2);
  
  do {
    /* Attesa di una connessione */	 
      
    msgsock= accept(sock,(struct sockaddr *)&client,(socklen_t *)&length);
      
    if(msgsock ==-1)
      perror("accept");
    else
    { 
      printf("Connessione da %s, porta %d\n", 
             inet_ntoa(client.sin_addr), ntohs(client.sin_port));
	 

      if((pid = fork())== 0)
      {
        write(msgsock,hellomsg,strlen(hellomsg)+1);
        read(msgsock,&buffer,sizeof(buffer));

        sscanf(buffer,"%s %d %d", comando, &indirizzo, &valore);
        if(strcmp(comando, "STORE") == 0)
        {
        	mesg.address = indirizzo;
        	mesg.value = valore;
        	write(pipe_store[1], &mesg, sizeof(mesg));
        }

        close(msgsock);
        exit(0);     
      }
      else
      {
        if(pid == -1)	/* Errore nella fork */
        {
          perror("Error on fork");
          exit(6);
        }
        else
	{	
          /* OK, il padre torna in accept */
          close(msgsock);
        }
      }	  
    }
      
  }
  while(1);
  exit(0);
}
