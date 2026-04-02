#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>

extern int errno;

static int sd = -1;
static int port;
static pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_send_and_receive(const char* command, char* raspuns, int raspuns_size) 
{
    if(sd == -1) 
    {
        printf("socket neconectat\n");
        fflush(stdout);
        return -1;
    }
    
    pthread_mutex_lock(&socket_mutex);
    
    printf("clientul scrie\n");
    fflush(stdout);

    size_t cmd_len = strlen(command);
    ssize_t scrie = write(sd, command, cmd_len);
    
    if(scrie <= 0) 
    {
        perror("eroare la write");
        pthread_mutex_unlock(&socket_mutex);
        fflush(stdout);
        return -1;
    }
    
    printf("clientul a scris\n");
    fflush(stdout);
    
    printf("clientul asteapta raspuns\n");
    fflush(stdout);
    
    memset(raspuns, 0, raspuns_size);
    ssize_t bytes = read(sd, raspuns, raspuns_size - 1);
    
    pthread_mutex_unlock(&socket_mutex);
    
    if(bytes <= 0) 
    {
        printf("eroare la citire\n");
        perror("eroare la read");
        fflush(stdout);
        return -1;
    }
    
    raspuns[bytes] = '\0';
    
    printf("clientul a primit raspuns\n");
    fflush(stdout);
    
    return bytes;
}

int client_send(const char* command) 
{
    if(sd == -1) 
    {
        printf("socket neconectat!\n");
        fflush(stdout);
        return -1;
    }
    
    pthread_mutex_lock(&socket_mutex);
    
    size_t cmd_len = strlen(command);
    
    printf("clientul scrie\n");
    fflush(stdout);
    
    ssize_t scrie = write(sd, command, cmd_len);
    
    pthread_mutex_unlock(&socket_mutex);
    
    if(scrie <= 0) 
    {
        perror("eroare la write");
        fflush(stdout);
        return errno;
    }
    
    printf("clientul a scris\n");
    fflush(stdout);
    
    return 0;
}

int client_receive(char* raspuns, int raspuns_size)
{
    if(sd == -1) 
    {
        printf("socket neconectat!\n");
        fflush(stdout);
        return -1;
    }
    
    pthread_mutex_lock(&socket_mutex);
    
    printf("clientul asteapta raspuns\n");
    fflush(stdout);
    
    memset(raspuns, 0, raspuns_size); 
    ssize_t bytes = read(sd, raspuns, raspuns_size - 1);  
    
    pthread_mutex_unlock(&socket_mutex);
    
    if(bytes <= 0) 
    {
        printf("eroare la citire\n");
        perror("eroare la read");
        fflush(stdout);
        return -1;
    }
    
    raspuns[bytes] = '\0'; 
    
    printf("clientul a primit raspuns (%d bytes)\n", (int)bytes);
    fflush(stdout);
    
    return bytes;
}

int client_connect(const char* server_addr, int server_port) 
{
    struct sockaddr_in server;
    port = server_port;
    
    printf("conectare la %s %d\n", server_addr, server_port);
    fflush(stdout);
    
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
        perror("eroare la socket");
        fflush(stdout);
        return errno;
    }
    
    printf("socket creat sd:%d\n", sd);
    fflush(stdout);
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(server_addr);
    server.sin_port = htons(port);
    
    if(connect(sd, (struct sockaddr*)&server, sizeof(struct sockaddr)) == -1) 
    {
        perror("eroare la connect");
        fflush(stdout);
        return errno;
    }
    
    printf("client conectat\n");
    fflush(stdout);
    
    return 0;
}

void client_close() 
{    
    if(sd != -1) 
    {
        printf("inchid conexiunea socket %d\n", sd);
        fflush(stdout);
        close(sd);
        sd = -1;
    }
}
