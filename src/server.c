#include "socketwrappers.h"
#include "wrapper.h"
#include "common.h"
#include "limits.h"
#include <pthread.h>

#define SERV_PORT 8000
#define LISTENQ 5
#define BUFSIZE 1024
#define MAX_THREADS USHRT_MAX 

void* echo_t(void* connfd);

void server(const char* port) {
    int listenfd, * connfd, n;
    struct sockaddr_in cliaddr, servaddr;
    socklen_t clilen;
    char buf[BUFSIZE];
    pid_t childpid;
    pthread_t threads[MAX_THREADS];
 
    listenfd = make_bound(port);
    Listen(listenfd, LISTENQ);  //change to higher number

    //should call waitpid()
    for(int i = 0; i < MAX_THREADS; i++) {
        clilen = sizeof(cliaddr);
        connfd = malloc(sizeof(int));
        if((*connfd = Accept(listenfd, (struct sockaddr*) &cliaddr, &clilen)) < 0) {
            if(errno == EINTR) {    //restart from interrupted system call
                continue;
            } else {
                puts("accept error");
            }
        }
        
        if(pthread_create(&threads[i], NULL, echo_t, (void*) connfd) != 0) {
            puts("thread creation failed");
            exit(1);
        }
        pthread_join(threads[i], NULL);
    }
}

void *echo_t(void *fd) {
    int n;
    int connfd = *((int*)fd);
    char buf[BUFSIZE];

    while(1) {
        if((n = read(connfd, buf, BUFSIZE)) < 0) {
            if(errno == ECONNRESET) {
                //connection reset
                puts("connection closed");
                close(connfd);
            } else if (errno == EAGAIN) {
                perror("EAGAIN");  
            } else {
                perror("reading error");
            }
        } else {
            write(connfd, buf, n);
        }
    }
}




