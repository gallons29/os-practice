#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define PORTNO 8123


int main(int argc, char* argv[])
{
  int	sock,length;
  struct  sockaddr_in 	server;
  int	pid,msgsock;
  struct	hostent	*server_addr, *gethostbyname();
	int cas, n;
  char send_buffer[256],rec_buffer[256];
  char *hellomsg = "Immettere un incremento intero per il contatore condiviso\n";
  

  /* Crea la  socket STREAM */
  sock=	socket(AF_INET,SOCK_STREAM,0);
  if(sock<0)
  {	
    perror("opening stream socket");
    exit(2);
  }

  
  /* Utilizzo della wildcard per accettare le connessioni ricevute da ogni interfaccia di rete del sistema */
  server.sin_family = 	AF_INET;
  server.sin_port = htons(PORTNO);
  server_addr = gethostbyname("localhost");
  if(server_addr == NULL)
  {
  	fprintf(stderr, "ERRORE: host sconosciuto\n");
  	exit(3);
  }
  
  memcpy((char *)&server.sin_addr,(char *)server_addr->h_addr,server_addr->h_length);
  if(connect(sock, (struct sockaddr *)&server,sizeof(server)) < 0)
  {
  	if(errno == ECONNREFUSED)
  	{
			  //PRIMA ISTANZA
				printf("Sono la prima istanza\n");
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
				msgsock= accept(sock,(struct sockaddr *)NULL,(int *)NULL);
				if(msgsock ==-1)
					perror("accept");
					
				read(msgsock, &rec_buffer, sizeof(rec_buffer));
				sscanf(rec_buffer, "%d", &n);
				printf("Ricevuto %d, il quadrato e' %d\n", n, n*n);
					

				exit(0);
		}
  }

  
  
  //SECONDA ISTANZA
	printf("Sono la seconda istanza\n");
	srand(time(NULL));
	cas = rand()%(10) + 1;
	sprintf(send_buffer,"%d", cas);
	write(sock, &send_buffer, sizeof(send_buffer));
	exit(0);
}
