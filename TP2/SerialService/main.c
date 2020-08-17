#include <stdio.h>
#include <stdlib.h>
#include "SerialManager.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#define PORT_NUMBER	1
#define BAUD_RATE	115200

//Creamos threads
static pthread_t thread_socket, thread_serial, thread_cancel;

//Crea mutex
pthread_mutex_t mutexData = PTHREAD_MUTEX_INITIALIZER;

//Estructuras para señales
struct sigaction signal_sigint;
struct sigaction signal_sigterm;

//Creacion de variables
int32_t uart_conn;
int32_t read_bytes;

socklen_t addr_len;
struct sockaddr_in clientaddr;
struct sockaddr_in serveraddr;
char buffer[128];
char N, X, Y, W, Z;
char buf_to_ciaa[30];
char buf_from_ciaa[30];
char buf_to_web[30];
char buf_from_web[30];
char serial_cmd[] = ">OUTS:";
char web_cmd[] = ":LINE";
int newfd;
int n, s;
volatile sig_atomic_t signal_flag; 

//Handler signals
void signals_handler(int sig)
{
	write(1,"Se presiono ctrl+c!!\n",21);
    
    signal_flag = 1;   

}

//Funcion para bloquear señales
void blockSignals(void)
{
    sigset_t set;
    if(sigemptyset(&set) == -1){
        perror("sigemptyset");
    }
    if(sigaddset(&set, SIGINT) != 0){
        perror("SIGINT error");
    }
    if(sigaddset(&set, SIGTERM) != 0){
        perror("SIGTERM error");
    }
    //sigaddset(&set, SIGUSR1);
    if(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0){
        perror("SIGINT block error");
    }
}

//Funcion para desbloquear señales
void unblockSignals(void)
{
    sigset_t set;
    if(sigemptyset(&set) == -1){
        perror("sigemptyset");
    }
    if(sigaddset(&set, SIGINT) != 0){
        perror("SIGINT error");
    }
    if(sigaddset(&set, SIGTERM) != 0){
        perror("SIGTERM error");
    }
    //sigaddset(&set, SIGUSR1);
    if(pthread_sigmask(SIG_UNBLOCK, &set, NULL) != 0){
        perror("SIGINT unblock error");
    }
}

//Handler para thread de socket
void* socket_handler (void* message)
{
    
    // Crea socket
	if((s = socket(PF_INET,SOCK_STREAM, 0)) == -1){
        perror("error socket cration");
    }

	// Cargamos datos de IP:PORT del server
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(10000);
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(serveraddr.sin_addr.s_addr==INADDR_NONE)
    {
        fprintf(stderr,"ERROR invalid server IP\r\n");
        //return 1;
    }

	// Abrimos puerto con bind()
	if (bind(s, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) == -1) {
		close(s);
		perror("listener: bind");
		//return 1;
	}

	// Seteamos socket en modo Listening
	if (listen (s, 10) == -1) // backlog=10
  	{
    	perror("error en listen");
    	exit(1);
  	}
	
	while(1)
	{
		// Ejecutamos accept() para recibir conexiones entrantes
		addr_len = sizeof(struct sockaddr_in);
    	if ( (newfd = accept(s, (struct sockaddr *)&clientaddr, &addr_len)) == -1){
			perror("error en accept");
			exit(1);
	    }
	 	
		//Imprime en consola ip de cliente
		char ipClient[32];
		inet_ntop(AF_INET, &(clientaddr.sin_addr), ipClient, sizeof(ipClient));
		printf  ("server:  conexion desde:  %s\n",ipClient);

        //Queda en este bucle mientras no se corte la conexion el cliente
		while(1){
            
            //read bloqueante a la espera de una trama
            //Inicio de mutex para proteger variable global "newfd"
            pthread_mutex_lock (&mutexData);
			
            n = read(newfd, buf_from_web, 128);
            
            pthread_mutex_unlock (&mutexData);
            //Fin mutex
            
            if(n == -1 ){
				perror("Error leyendo mensaje en socket\n");
				exit(1);
			}
			else if(n == EOF){
                newfd = -1;
				printf("EOF\n");
				break;
			}
			else{
				
                
                buf_from_web[n]=0x00;
				printf("Recibi %d bytes.:%s", n, buf_from_web);
                
                sscanf(buf_from_web, ":STATES%c%c%c%c\n", &X, &Y, &W, &Z);
                
                printf("X:%c, Y:%c, W:%c, Z:%c\n\n", X, Y, W, Z);
                
                //Arma string “>OUTS:X,Y,W,Z\r\n”
                sprintf(buf_to_ciaa, "%s%c,%c,%c,%c\r\n", serial_cmd, X, Y, W, Z);
                
                //Envío de trama a EDU-CIAA
                serial_send(buf_to_ciaa, strlen(buf_to_ciaa));
                
                usleep(100);
                
                               
			}
		}

    	close(newfd);
	}
	
	//Cierra socket
	close(s);

    return NULL;
}

//Handler para thread de puerto serial
void* serial_handler (void* message)
{
    
    //Loop infinito para recibir tramas de la EDU-CIAA
 	while(1){
		if((read_bytes = serial_receive(buf_from_ciaa, 30)) <= 0){
            
		}
		else{
			
            buf_from_ciaa[read_bytes] = 0x00;
            if(buf_from_ciaa[1] == 'O'){
                //Acá no hace nada
            }
            else{
                
                sscanf(buf_from_ciaa, ">TOGGLE STATE:%c\r\n", &N);
                
                //Arma string “>OUTS:X,Y,W,Z\r\n”
                sprintf(buf_to_web, "%s%cTG\n", web_cmd, N);
                
                printf("Trama web: %s", buf_to_web);
                
                //Verifica conexion
                //Inicia mutex para proteger variable global "newfd"
                pthread_mutex_lock (&mutexData);
                
                if(newfd == -1){
                    printf("Se cerro conexion\n");
                    exit(1);
                }
                // Envia mensaje a servidor web / Chequeo de errores
                if (write (newfd, buf_to_web, 9) == -1){                    
                    perror("Error escribiendo mensaje en socket");
                    exit (1);
                }               
                usleep(100);
                
                pthread_mutex_unlock (&mutexData);
                //Fin mutex
            }
            
			//printf("Trama: %d bytes. %s", read_bytes, buf_from_ciaa);
		}
		
		usleep(10000);
	}
	
	return NULL;
}

int main(void)
{

   	//Verifica conexion con puerto serial
	if((uart_conn = serial_open(PORT_NUMBER, BAUD_RATE)) < 0){
		printf("Error al iniciar puerto\r\n");
		return 0;
	};
	
	printf("Puerto: %d\n", uart_conn);
    
    //Signal SIGINT
    signal_sigint.sa_handler = signals_handler;    
	signal_sigint.sa_flags = 0;
	if(sigemptyset(&signal_sigterm.sa_mask) != 0){
        perror("sigemptyset signal_sigint");
    }
    if (sigaction(SIGINT, &signal_sigint, NULL) == -1) {
		perror("sigaction signal_sigint");
		exit(1);
	}

    //Signal SIGTERM
    signal_sigterm.sa_handler = signals_handler;    
	signal_sigterm.sa_flags = 0;
	if(sigemptyset(&signal_sigterm.sa_mask) != 0){
        perror("sigemptyset signal_sigterm");
    }
	if(sigaction(SIGINT, &signal_sigterm, NULL) == -1) {
		perror("sigaction signal_sigterm");
		exit(1);
	}

	//Bloqueo de signals
	blockSignals();

    //Creacion de threads
    //Thread socket
	if(pthread_create (&thread_socket, NULL, socket_handler, NULL) != 0){
        perror("Error al crear thread_socket\n");
        exit (1);
    }
	//Thread puerto serial
	if(pthread_create (&thread_serial, NULL, serial_handler, NULL) != 0){
        perror("Error al crear thread_serial\n");
        exit (1);
    }
    
    //Desbloqueo de signals
	unblockSignals();
    
    printf("Programa corriendo...\n");
    
    while(!signal_flag){
        usleep(1000);
    }
    
    //Se cancelan los threads antes del join
    //pthread_cancel de thread_socket
    if(pthread_cancel(thread_socket) != 0){
        perror("Error al cancelar thread_socket\n");
        exit (1);
    }
    //pthread_cancel de thread_serial
    if(pthread_cancel(thread_serial) != 0){
        perror("Error al cancelar thread_serial\n");
        exit (1);
    } 
    
    //Join threads
	//Join thread_socket
 	void* ret;
    if(pthread_join (thread_socket, &ret) != 0){
        perror("join thread_socket");
    }
	if(ret==PTHREAD_CANCELED)
		printf("Se cancelo thread_socket\n");
	else
		printf("Termino thread_socket\n");

    //Join thread_serial
	if(pthread_join (thread_serial, &ret) != 0){
        perror("join thread_serial");
    }
	if(ret==PTHREAD_CANCELED)
		printf("Se cancelo thread_serial\n");
	else
		printf("Termino thread_serial\n");	
	
    //Cierra puerto COM
    serial_close();
    
    //Fin del programa
    printf("Fin\n");    	
	
	exit(EXIT_SUCCESS);
	return 0;
}
