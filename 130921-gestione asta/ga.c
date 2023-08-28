#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#define PORTNO 1234




int main(int argc, char **argv)
{
	int nMax;
	if(argc > 1)
	{
		nMax = atoi(argv[1]);
	} else {
		nMax = 20;
	}

	srand(time(NULL));
	int nRandom = rand()%nMax + 1;

  int sock,length,msgsock,rval, pid, user_value;
  struct sockaddr_in server, client;	
  struct hostent *hp;
  
  int pipeval[2];
  char buffer[256],answer[256];
  
  char *hellomsg = "Metti er numero\n";
  
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
  
  printf("Socket port #%d\n",ntohs(server.sin_port));
  
 // PIPE  
  if (pipe(pipeval)<0) {
      perror("creating pipe");
      exit(5);
  }
  
  write(pipeval[1],&nRandom,sizeof(nRandom));
  
  listen(sock, 5);
  
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
      	//sprintf(hellomsg, "Immettere un numero tra 1 e %d\n", nMax);
        write(msgsock,hellomsg,strlen(hellomsg)+1);
        read(msgsock,&buffer,sizeof(buffer));

        sscanf(buffer,"%d",&user_value);
        
        /* Uso della pipe per recuperare nRandom */
        read(pipeval[0],&nRandom,sizeof(nRandom));

				if(user_value == nRandom)
				{
					nRandom = rand()%nMax + 1;
					write(pipeval[1],&nRandom,sizeof(nRandom));
		      sprintf(answer,"Hai indovinato!\nNumero resettato. Immettere un numero tra 1 e %d\n", nMax);

				} else {
					strcpy(answer, "Ritenta!\n");
				}

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

