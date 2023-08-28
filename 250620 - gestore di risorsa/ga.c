#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define PORTNO 8123
#define TRUE (1 == 1)
#define FALSE (1 == 0)


int servizio_attivo = TRUE;

typedef struct _waitpid{
	int tabwp[256];
	int size_tabwp;
} WAITPID;

void handler(int signo)
{
	if(signo == SIGUSR1)
	{
		if(servizio_attivo == TRUE)
		{
			printf("Ricevuto SIGUSR1. Servizio disabilitato\n");
			servizio_attivo = FALSE;
		}
		else if (servizio_attivo == FALSE)
		{
			printf("Ricevuto SIGUSR1. Servizio abilitato\n");
			servizio_attivo = TRUE;
		}
	}

}

int main(int argc, char* argv[])
{
  int	sock,length;
  struct  sockaddr_in 	server,client;
  int	pid,msgsock,i;
  struct	hostent	*hp,*gethostbyname();
  char buffer[256],answer[256];
  char *hellomsg = "Scrivi \"P\" per richiedere la risorsa\n";
  int rid, urid;
  char command[256];
  int pipetab[2];
  WAITPID wp;
  
  sigset_t zeromask;
  struct sigaction action;
  sigemptyset(&zeromask);
  sigemptyset(&action.sa_mask);
  action.sa_handler = handler;
  action.sa_flags = 0;
  if(sigaction(SIGUSR1, &action, NULL) == -1)
  	perror("sigaction error");
  
  
  
  wp.size_tabwp=0;
  

  /* Crea la  socket STREAM */
  sock=	socket(AF_INET,SOCK_STREAM,0);
  if(sock<0)
  {	
    perror("opening stream socket");
    exit(2);
  }
  
  /* Utilizzo della wildcard per accettare le connessioni ricevute da ogni interfaccia di rete del sistema */
  server.sin_family = 	AF_INET;
  server.sin_addr.s_addr= INADDR_ANY;
  server.sin_port = htons(PORTNO);
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
  
  printf("Socket port #%d PID #%d\n",ntohs(server.sin_port), getpid());
  
	/* Pipe */
	if(pipe(pipetab)<0)
	{
		perror("errore pipe");
		exit(4);
	}
	write(pipetab[1], &wp, sizeof(wp));

  
  /* Pronto ad accettare connessioni */
  
  listen(sock,4);
  
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
      	if(servizio_attivo)
      	{
		      write(msgsock,hellomsg,strlen(hellomsg)+1);
		      read(msgsock,&buffer,sizeof(buffer));

		      sscanf(buffer,"%s",command);
					if(strcmp(command, "P") == 0)
					{
						srand(time(NULL));
						
						read(pipetab[0], &wp, sizeof(wp));
						wp.size_tabwp++;
						wp.tabwp[wp.size_tabwp-1] = getpid();

						if(wp.tabwp[0] != getpid())
						{
							printf("Dentro if risorsa lib\n");
							sprintf(answer, "Risorsa occupata\n");
							write(msgsock,answer,strlen(answer)+1);		
						}
						while(wp.tabwp[0] != getpid())
						{
							sleep(1); //In modo che non perma controlli per usare meno risorse
						}
						//Risorsa libera per il processo giusto
						rid = rand()%(1000002) + 1;
						
						sprintf(answer, "Risorsa assegnata: ID = %d\n", rid);
						write(msgsock,answer,strlen(answer)+1);
						
						while(1)
						{
							read(msgsock,&buffer,sizeof(buffer));
							sscanf(buffer,"%s %d",command, &urid);
							if(strcmp(command, "R") || urid != rid)
							{
								sprintf(answer, "Errore. Per rilasciare la risorsa fare: R %d\n", rid);
								write(msgsock,answer,strlen(answer)+1);
							}
							else
							{ //Rilascia
								for(i=0; i<wp.size_tabwp; i++)
								{
									wp.tabwp[i] = wp.tabwp[i+1];
								}
								wp.size_tabwp--;
								write(pipetab[1], &wp, sizeof(wp));
								sprintf(answer, "Risorsa liberata, ciao\n");
								write(msgsock,answer,strlen(answer)+1);
								close(msgsock);
						    exit(0); 
							}
						}
						
					}
					else
					{
						sprintf(answer,"Per richiedere la risorsa dovevi scrivere P\n");
						write(msgsock,answer,strlen(answer)+1);
					}
		
		      close(msgsock);
		      exit(0);   
		    }
		    else
		    {
		    	sprintf(answer,"Servizio non attivo\n");
					write(msgsock,answer,strlen(answer)+1);
		    	close(msgsock);
		      exit(0);  
		    }  
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
