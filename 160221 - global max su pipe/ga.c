#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

int max_value, tracker = 0;

void handler(int signo)
{
	if(signo == SIGUSR1)
	{
		tracker = 1;
	} else if(signo == SIGUSR2) {
		char usr2msg[256];
		sprintf(usr2msg, "Max = %d\nMax resettato\n", max_value);
		write(1, &usr2msg, strlen(usr2msg));
		max_value = 0;
	}
}


int main(int argc, char* argv[])
{
  int	sock,length,portno;
  struct  sockaddr_in 	server,client;
  int	pid,msgsock;
  struct	hostent	*hp,*gethostbyname();
  int p,q;
  char buffer[256],answer[256];
  char *hellomsg = "Immettere due numeri interi positivi\n";
  sigset_t set, zeromask;
  struct sigaction action;
  
  sigemptyset(&zeromask);
  sigemptyset(&action.sa_mask);
  action.sa_handler = handler;
  action.sa_flags = 0;
  
  
  
  if (argc !=2) {
    printf("Uso: %s <numero_porta>\n", argv[0]);
    exit(1);
  }
  
  if(sigaction(SIGUSR1, &action, NULL) == -1)
  {
  	perror("sigaction error");
  }
    if(sigaction(SIGUSR2, &action, NULL) == -1)
  {
  	perror("sigaction error");
  }

  /* Crea la  socket STREAM */
  sock=	socket(AF_INET,SOCK_STREAM,0);
  if(sock<0)
  {	
    perror("opening stream socket");
    exit(2);
  }
  
  portno = atoi(argv[1]);
  
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
  
  /* Creazione di una pipe per ospitare il messaggio con il valore corrente del contatore */
  /*if (pipe(pipemax)<0) {
        perror("creating pipe");
        exit(5);
    }*/
  
  max_value = 0;
//  write(pipemax[1],&max_value,sizeof(max_value));
  
  
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

        sscanf(buffer,"%d %d",&p, &q);
        
        if(p > q)
        {
        	sprintf(answer, "%d > %d\n", p, q);
        } else if(q > p) {
        	sprintf(answer, "%d > %d\n", q, p);
        } else {	
        	sprintf(answer, "%d = %d\n", p, q);
        }
        
        write(msgsock, answer, strlen(answer)+1);
      //  read(pipemax[0],&max_value,sizeof(max_value));
				if(p>max_value && tracker) max_value=p;
				if(q>max_value && tracker) max_value=q;
        //write(pipemax[1],&max_value,sizeof(max_value));
			
				if(tracker)
				{
        	sprintf(answer,"Il massimo globale ora vale %d\n",max_value);
        	write(msgsock,answer,strlen(answer)+1);
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
