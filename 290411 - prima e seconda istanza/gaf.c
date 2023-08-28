#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

char *pathrizio = "/tmp/fifo.giacomo";

int main()
{
	int cas, n=0;
	int fifo_server, fifo_client;
	
	if((fifo_client = open(pathrizio, O_WRONLY)) < 0)
	{
		//Prima istanza
		printf("Prima istanza\n");
		
		if(mkfifo(pathrizio, 0622) < 0)
		{
			perror("errore fifo");
			exit(1);
		}
		
		fifo_server = open(pathrizio, O_RDONLY);
		if(fifo_server<0)
		{
			perror("errore lettura fifo server");
			exit(2);
		}
		
		read(fifo_server, &n, sizeof(int));
		
		printf("Prima ist: %d al quadrato = %d\n",n,n*n);
		
		close(fifo_server);
		unlink(pathrizio);
		exit(0);
		
	}
	else
	{
		//Seconda istanza
		srand(time(NULL));
		
		cas = rand()%(10) + 1;
		
		write(fifo_client, &cas, sizeof(int));
		printf("seconda ist: ho inviato %d\n", cas);
		close(fifo_client);
		exit(0);
	}
	

	exit(0);
}
