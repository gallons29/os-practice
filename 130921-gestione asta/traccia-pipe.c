
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/timeb.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#define PORT 4567 //la porta deve essere >= 1024 per essere utilizzabile dagli utenti


int msgsock;

void handler(int signo) {
        char *vinto="Hai vinto l'asta!\n";
        write(msgsock,vinto,strlen(vinto)+1);
        kill(0,SIGQUIT);
}



int main(int argc,char *argv[]) {
char buf[80];
struct sockaddr_in server, client;
int pipemax[2],offerta,max,length,pidf,t;
int sock;
char *offertaAlta="Al momento la tua offerta è la più alta. Attendi";
char *altraOfferta="\nLa tua offerta e' stata superata. Fai un rilancio\n";
char *faiOfferta="Fai la tua offerta\n";

signal(SIGALRM,handler);

pipe(pipemax);

max=0;
write(pipemax[1],&max,sizeof(max));
	//creo la socket stream
	sock = socket(AF_INET,SOCK_STREAM,0);
	if(sock<0)
	{
		perror("opening stream socket");
		exit(1);
	}

int on=1;
    if (setsockopt(sock,SOL_SOCKET, SO_REUSEADDR,&on,sizeof(on))<0)
            perror("setsockopt reuseaddr:");


	
	server.sin_family = AF_INET;
	server.sin_addr.s_addr= INADDR_ANY;
	server.sin_port = htons(4567);
	if (bind(sock,(struct sockaddr *)&server,sizeof server)<0)
	{
		perror("binding stream socket");
		exit(1);
	}

	length= sizeof(server);
	if(getsockname(sock,(struct sockaddr *)&server,&length)<0)
	{
		perror("getting socket name");
		exit(1);
	}
	printf("Socket port #%d\n",ntohs(server.sin_port));

	listen(sock,10);

do {

    msgsock = accept(sock, (struct sockaddr *) &client, (socklen_t *) &length);
		if(msgsock < 0)
		{
			perror("errore nella accept della connessione\n");
			exit(1);
		}
		printf("Connessione avvenuta con successo col client %s\n", inet_ntoa(client.sin_addr));
		
		if((pidf = fork()) < 0)
		{
			perror("errore avvio processo server concorrente\n");
			exit(1);
		}
		else if(pidf == 0) {

            write(msgsock,faiOfferta,strlen(faiOfferta));
            
            read(msgsock,buf,sizeof(buf));

            sscanf(buf,"%d",&offerta);

        do {
    
            read(pipemax[0],&max,sizeof(max));
            printf("Offerta max=%d\n",max);
    
            if (offerta>max) {
                write(msgsock,offertaAlta,strlen(offertaAlta));
                alarm(60);
                t=60;
                write(pipemax[1],&offerta,sizeof(offerta));
                // aspetto aggiornamento via pipe
                sleep(1);
                }
            else if (offerta<max){ // rilancio
                alarm(0);
                
                write(pipemax[1],&max,sizeof(max));
                sprintf(buf,"Offerta piu' alta: %d",max);
                write(msgsock,buf,strlen(buf));
                write(msgsock,altraOfferta,strlen(altraOfferta));
                read(msgsock,buf,sizeof(buf));
                sscanf(buf,"%d",&offerta);
                }
     else {
          sprintf(buf,"%s : rimangono %d secondi\n",offertaAlta,--t);
          write(msgsock,buf,strlen(buf));
          write(pipemax[1],&max,sizeof(max));
          sleep(1);
          
     }         
     
           
    } while (1);
    
  }  
else {
    close(msgsock);
} 

} while(1);

}    
  
  