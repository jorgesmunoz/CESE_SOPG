#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#define FIFO_NAME "myfifo"
#define BUFFER_SIZE 300

int main(void)
{
	uint8_t inputBuffer[BUFFER_SIZE];
	int32_t bytesRead, returnCode, fd, i;

	FILE *sign;
	FILE *log;
    
    /* Create named fifo. -1 means already exists so no action if already exists */
    if ( (returnCode = mknod(FIFO_NAME, S_IFIFO | 0666, 0) ) < -1  )
    {
        printf("Error creating named fifo: %d\n", returnCode);
        exit(1);
    }
    
    /* Open named fifo. Blocks until other process opens it */
	printf("waiting for writers...\n");
	if ( (fd = open(FIFO_NAME, O_RDONLY) ) < 0 )
    {
        printf("Error opening named fifo file: %d\n", fd);
        exit(1);
    }
    
    /* open syscalls returned without error -> other process attached to named fifo */
	printf("got a writer\n");

    /* Loop until read syscall returns a value <= 0 */
	do
	{
        /* read data into local buffer */
		if ((bytesRead = read(fd, inputBuffer, BUFFER_SIZE)) == -1)
        {
			perror("read");
        }
        else
		{
			inputBuffer[bytesRead] = '\0';
			printf("%c\n", inputBuffer[0]);
			printf("reader: read %d bytes: \"%s\"\n", bytesRead, inputBuffer);
			if(inputBuffer[0] == 'D'){
				printf("Entra a data\n");
				log = fopen ("Log.txt", "a");
				for (i = 0; inputBuffer[i] != '\0'; i++) {
				/* write to file using fputc() function */
					fputc(inputBuffer[i], log);
				}
				fclose(log);
			}
			else if(inputBuffer[0] == 'S'){
				printf("Entra a signal\n");
				sign = fopen ("Sign.txt", "a");
				for (i = 0; inputBuffer[i] != '\0'; i++) {
				/* write to file using fputc() function */
					fputc(inputBuffer[i], sign);
				}
				fclose(sign);						
			}
			else{
				perror("reading");
			}
			

		}
	}
	while (bytesRead > 0);

	return 0;
}
