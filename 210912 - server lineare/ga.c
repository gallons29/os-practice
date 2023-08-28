#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

int q;

void handler(int signo)
{
	if(signo == SIGUSR1)
	{
		q++;
		printf("Ricevuto SIGUSR1, q=%d\n", q);
	}
	if(signo == SIGUSR2)
	{
		q--;
		printf("Ricevuto SIGUSR2, q=%d\n", q);
	}


}

int main(int argc, char* argv[])
{
  int	sock,length,portno;
  struct  sockaddr_in 	server,client;
  int	pid,msgsock,i;
  struct	hostent	*hp,*gethostbyname();
  int dato, m=1, result;
  char buffer[256],answer[256];
  char *hellomsg = "Immettere il dato\n";
  
  sigset_t zeromask;
  struct sigaction action;
  
  if (argc !=3 || atoi(argv[2])<0 || atoi(argv[2])>100) {
    printf("Uso: %s <numero_porta> <[0-100]>\n", argv[0]);
    exit(1);
  }
  
  /* Segnali */
  sigemptyset(&zeromask);
  sigemptyset(&action.sa_mask);
  action.sa_handler = handler;
  action.sa_flags = 0;
  
  if(sigaction(SIGUSR1, &action, NULL) == -1)
  	perror("Sigaction error");
  if(sigaction(SIGUSR2, &action, NULL) == -1)
  	perror("Sigaction error");
  
  

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
  
  printf("Socket port #%d, PID #%d\n",ntohs(server.sin_port), getpid());
  
  /* Creazione di una pipe per ospitare il messaggio con il valore corrente del contatore */
  /*if (pipe(pipeq)<0) {
        perror("creating pipe");
        exit(5);
    }*/
  // Valore iniziale del contatore
  q = atoi(argv[2]);
  //write(pipeq[1],&q,sizeof(q));
  
  
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
        sscanf(buffer,"%d",&dato);
				
				
				if(dato%100 == 0)
				{
					m = dato/100;
				}
        
        result = (m * dato) + q;
        
        sprintf(answer,"M (%d) * dato (%d) + q (%d) = %d\n", m, dato, q, result);

        write(msgsock,answer,strlen(answer)+1);
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
