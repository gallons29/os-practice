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

typedef struct _libro{
	int idLibro;
	char titolo[50];
	char autore[50];
	int disponibile;
	int codice;
} LIBRO;

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
  char *hellomsg = "--------------------------------\n\"L\" per visualizzare la lista.\n\"P <idlibro> \" per richiedere il libro\n\"F\" per terminare\n\"R\" <idlibro> <codice> per restituire\n--------------------------------\n";
  int uid, rcod, rucod;
  char command[256];
  int pipelib[2];
  
  if(argc != 2 || atoi(argv[1])<4 || atoi(argv[1])>10)
  {
  	printf("Uso: %s <[4-10]>\n", argv[0]);
  	exit(-1);
  }
  
  int n_libri = atoi(argv[1]);
  LIBRO biblioteca[n_libri];
  /* Seed libri */
  for(i=0; i<n_libri; i++)
  {
  	LIBRO tmp_lib;
  	tmp_lib.idLibro = i;
  	sprintf(tmp_lib.titolo, "Titolo%d", i);
  	sprintf(tmp_lib.autore, "Autore%d", i);
  	tmp_lib.disponibile = TRUE;
  	biblioteca[i] = tmp_lib;
  }
  
  /* Gestione segnali */
  sigset_t zeromask;
  struct sigaction action;
  sigemptyset(&zeromask);
  sigemptyset(&action.sa_mask);
  action.sa_handler = handler;
  action.sa_flags = 0;
  if(sigaction(SIGUSR1, &action, NULL) == -1)
  	perror("sigaction error");
  	
  if(pipe(pipelib)<0)
  {
  	perror("errore pipe");
  	exit(5);
  }
  write(pipelib[1], &biblioteca, sizeof(biblioteca));
   

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
      		while(1)
      		{
				    write(msgsock,hellomsg,strlen(hellomsg)+1);
				    read(msgsock,&buffer,sizeof(buffer));

				    sscanf(buffer,"%s %d %d",command, &uid, &rucod);
				    
				    if(strcmp(command, "F") == 0)
				    {
				    	sprintf(answer,"Sessione terminata\n");
				    	write(msgsock,answer,strlen(answer)+1);
							close(msgsock);
				    	exit(0)		;		    	
				    }
						else if(strcmp(command, "P") == 0)
						{
							srand(time(NULL));
							read(pipelib[0], &biblioteca, sizeof(biblioteca));
							if(uid < 0 || uid > n_libri)
							{
								sprintf(answer, "ID non valido\n");
							}
							else if(biblioteca[uid].disponibile)
							{
								biblioteca[uid].disponibile=FALSE;
								rcod = rand()%(100000-10000) + 10000; //in realta bisognerebbe randomizzare ogni cifra perche cosi la prima cifra non sara mai 0
								biblioteca[uid].codice = rcod;
								sprintf(answer, "OK %d\n", rcod);
							}
							else
							{
								sprintf(answer, "Libro %d non disponibile\n", uid);
							}
							
							write(pipelib[1], &biblioteca, sizeof(biblioteca));
							write(msgsock,answer,strlen(answer)+1);
						}
						else if(strcmp(command, "R") == 0)
						{
							read(pipelib[0], &biblioteca, sizeof(biblioteca));
							if(uid < 0 || uid > n_libri)
							{
								sprintf(answer, "ID non valido\n");
							}
							else if(!biblioteca[uid].disponibile)
							{
								if(biblioteca[uid].codice == rucod)
								{
									sprintf(answer, "Libro %d restituito\n", uid);
									biblioteca[uid].disponibile = TRUE;
								}
								else
								{
									sprintf(answer, "Codice errato per il libro %d\n", uid);
								}
							}
							else
							{
								sprintf(answer, "Libro %d gia' disponibile\n", uid);
							}
							
							write(pipelib[1], &biblioteca, sizeof(biblioteca));
							write(msgsock,answer,strlen(answer)+1);
						}
						else if(strcmp(command, "L") == 0)
						{
							sprintf(answer,"Libri disponibili:\n");
							write(msgsock,answer,strlen(answer)+1);
							read(pipelib[0], &biblioteca, sizeof(biblioteca));
							for(i=0; i<n_libri; i++)
							{
								if(biblioteca[i].disponibile)
								{
									sprintf(answer,"ID %d: %s, %s\n", biblioteca[i].idLibro, biblioteca[i].titolo, biblioteca[i].autore);
									write(msgsock,answer,strlen(answer)+1);
								}
							}
							write(pipelib[1], &biblioteca, sizeof(biblioteca));
						}
						else
						{
						 	printf("Comando non valido\n");
						}   
				  }
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
