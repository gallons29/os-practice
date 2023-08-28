
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
int pipecmd[2], piperesult[2];
int fineAsta = 0;
int gestore=0;
int servizioSospeso=0;

int offerta=0,max=0,miaOfferta=0;

void handler(int signo) {
char *vinto="Hai vinto l'asta!\n";
char *nonvinto="Non hai vinto l'asta, mi dispiace\n";
    switch (signo) {
        case SIGUSR1:
            miaOfferta= 1;
            break;
        case SIGUSR2:
		    servizioSospeso = !servizioSospeso;
            miaOfferta=-1;
            break;
            
        case SIGALRM:
            fineAsta = 1;
            if (!gestore) {
                if (offerta > max) 
                    write(msgsock,vinto,strlen(vinto)+1);
                else
                    write(msgsock,nonvinto,strlen(nonvinto)+1);
                exit(0);
                
                }
                
    }       
                



}

int gestoreAsta() {
int max=0;
int value[2]; // PID, value
int offerenteMax =0;
int offertaMax =0;

gestore = 1;

    do {
             
        read(pipecmd[0],value,sizeof(value));
        if(fineAsta) {
            kill(0,SIGALRM);
            exit(0);
        }
        
        
        if (value[1] > max) {
            offertaMax = value[1];
            printf("Nuova offerta massima: %d\n", offertaMax);
            write(piperesult[1],&offertaMax,sizeof(offertaMax));
            kill(offerenteMax,SIGUSR2); 
            offerenteMax = value[0];
           
            kill(offerenteMax,SIGUSR1);
            alarm(60);
            }
        else {
           write(piperesult[1],&offertaMax,sizeof(offertaMax));
           kill(value[0],SIGUSR2);
        }
            
       
        
        
    } while(1);
    
    
}


int main(int argc,char *argv[]) {
char buf[80];
struct sockaddr_in server, client;
int length,pidf,t;
int sock;
char *offertaAlta="Al momento la tua offerta è la più alta. Attendi\n";
char *altraOfferta="\nLa tua offerta e' stata superata. Fai un rilancio\n";
char *faiOfferta="Fai la tua offerta\n";
char *msgServizioSospeso="Il servizio è momentaneamnte sospeso, riprova più tardi\n";
  struct sigaction act;
  sigset_t set, zeromask;
int mesg[2];


pipe(pipecmd);
pipe(piperesult);





 /* Gestione segnali */

  sigemptyset(&act.sa_mask);

  act.sa_handler = handler;
#ifdef SA_INTERRUPT
  act.sa_flags= SA_INTERRUPT;
#endif

  if(sigaction(SIGUSR1,&act, NULL)==-1)
    {
    perror("sigaction error");
    exit(4);
    }

  if(sigaction(SIGUSR2,&act, NULL)==-1)
    {
    perror("sigaction error");
    exit(4);
    }

  if(sigaction(SIGALRM,&act, NULL)==-1)
    {
    perror("sigaction error");
    exit(4);
    }

  /* Segnale SIGUSR1/2 inizialmente bloccati */

  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGUSR2);
  sigprocmask(SIG_BLOCK, &set, NULL);
  sigemptyset(&zeromask);

// Processo gestore
if (fork()==0) {
    gestoreAsta();
    exit(0);
    
}


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
	    if (servizioSospeso) {
			write(msgsock,msgServizioSospeso,strlen(msgServizioSospeso)+1);
			
		}			
		else
		if((pidf = fork()) < 0)
		{
			perror("errore avvio processo server concorrente\n");
			exit(1);
		}
		else if(pidf == 0) {

  write(msgsock,faiOfferta,strlen(faiOfferta));
            
  read(msgsock,buf,sizeof(buf));

            sscanf(buf,"%d",&offerta);
            mesg[0] = getpid();
            mesg[1]  = offerta;
            write(pipecmd[1],&mesg,sizeof(mesg));       

       
        do {
    
            

            sigsuspend(&zeromask);
            
            switch (miaOfferta) {
                case 1:
                    write(msgsock,offertaAlta,strlen(offertaAlta));
                    break;
                case -1:
                    read(piperesult[0],&max,sizeof(max));
                    sprintf(buf,"Offerta piu' alta: %d",max);
                    write(msgsock,buf,strlen(buf));
                    write(msgsock,altraOfferta,strlen(altraOfferta)); 
                    read(msgsock,buf,sizeof(buf));

                    sscanf(buf,"%d",&offerta);
                    mesg[0] = getpid();
                    mesg[1]  = offerta;
                    write(pipecmd[1],&mesg,sizeof(mesg));   
                    break;
                }    
    
           
          
            } while(1);         
     
           

    
  }  
else {
    close(msgsock);
} 

} while(1);

}    
  
  