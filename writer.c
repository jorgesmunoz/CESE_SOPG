#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#define FIFO_NAME "myfifo"
#define BUFFER_SIZE 300

int32_t flag;

void signusr1(int sig)
{
	//write(1,"se presiono SIGN:1\n",21);
	flag = 1;
}

void signusr2(int sig)
{
	//write(1,"se presiono SIGN:2\n",21);
	flag = 2;
}

int main(void)
{
   
    char outputBuffer[BUFFER_SIZE];
	uint32_t bytesWrote;
	int32_t returnCode, fd;
	char str_log[] = "DATA:";	

	struct sigaction sign1;
	struct sigaction sign2;

	sign1.sa_handler = signusr1;
	sign1.sa_flags = 0;
	sigemptyset(&sign1.sa_mask);

	sign2.sa_handler = signusr2;
	sign2.sa_flags = 0;
	sigemptyset(&sign2.sa_mask);

	sigaction(SIGUSR1,&sign1,NULL);
	sigaction(SIGUSR2,&sign2,NULL);

    /* Create named fifo. -1 means already exists so no action if already exists */
    if ( (returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0) ) < -1 )
    {
        printf("Error creating named fifo: %d\n", returnCode);
        exit(1);
    }

    /* Open named fifo. Blocks until other process opens it */
	printf("waiting for readers...\n");
	if ( (fd = open(FIFO_NAME, O_WRONLY) ) < 0 )
    {
        printf("Error opening named fifo file: %d\n", fd);
        exit(1);
    }
    
    /* open syscalls returned without error -> other process attached to named fifo */
	printf("got a reader--type some stuff\n");

    /* Loop forever */
	while (1)
	{
        /* Get some text from console */
		fgets(outputBuffer, BUFFER_SIZE, stdin);
				
		if(flag == 1){
			printf("se presiono SIGN:1\n");
			char str_sign1[] = "SIGN:1"; 
			if ((bytesWrote = write(fd, str_sign1, strlen(str_sign1))) == -1)
        		{
				perror("write");
        		}
        		else
        		{
				printf("writer: wrote %d bytes\n", bytesWrote);
        		}
			flag = 0;
		}
        	else if(flag == 2){
			printf("se presiono SIGN:2\n");
			char str_sign2[] = "SIGN:2"; 
			if ((bytesWrote = write(fd, str_sign2, strlen(str_sign2))) == -1)
        		{
				perror("write");
        		}
        		else
        		{
				printf("writer: wrote %d bytes\n", bytesWrote);
        		}
			flag = 0;
		}
		else{
			strncat( str_log, outputBuffer, strlen(outputBuffer));
			if ((bytesWrote = write(fd, str_log, strlen(str_log)-1)) == -1)
        		{
				perror("write");
        		}
        		else
        		{
				printf("writer: wrote %d bytes\n", bytesWrote);
        		}
		}
        /* Write buffer to named fifo. Strlen - 1 to avoid sending \n char */
		
	}
	return 0;
}
