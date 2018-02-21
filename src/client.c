#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include "client.h"
#include "common.h"
#include "logging.h"
#include "wrapper.h"

static volatile int running = 1;
static void handler(int sig) {
    running = 0;
}

void * client_handler(void * efd_ptr) {
    int efd = *((int *)efd_ptr);
    struct epoll_event *events;
    int bytes;

    // Buffer where events are returned (no more that 64 at the same time)
    events = calloc(MAXEVENTS, sizeof(struct epoll_event));

    while (running) {
        int n, i;
        //printf("current scale: %d\n",scaleback);
        n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) { // error or unexpected close
                lost_con(((connection*)events[i].data.ptr)->sockfd);
                close_connection(events[i].data.ptr);
                continue;
            } else {
                if (events[i].events & EPOLLIN) {//data has been echoed back or remote has closed connection
                    //puts("EPOLLIN");
                    bytes = black_hole_read((connection *)events[i].data.ptr);
                }

                if (events[i].events & EPOLLOUT) {//data can be written
                    //puts("EPOLLOUT");
                    bytes = white_hole_write((connection *)events[i].data.ptr);
                }
            }
        }
    }
}

void client(const char * address, const char * port, int rate) {
    int total_threads = get_nprocs();
    int * epollfds = calloc(total_threads, sizeof(int));
    connection * con;
    struct epoll_event event;
    int epoll_pos = 0;


    signal(SIGINT, handler);

    //make the epolls for the threads
    //then pass them to each of the threads
    for (int i = 0; i < total_threads; ++total_threads) {
        ensure((epollfds[i] = epoll_create1(0)) != -1);

        pthread_attr_t attr;
        pthread_t tid;

        ensure(pthread_attr_init(&attr) == 0);
        ensure(pthread_create(&tid, &attr, &client_handler, &epollfds[i]) == 0);
        ensure(pthread_attr_destroy(&attr) == 0);
        ensure(pthread_detach(tid) == 0);//be free!!
    }

    if (rate) {
        while (running) {
            usleep(rate);
            con = (connection *)malloc(sizeof(connection));
            init_connection(con, make_connected(address, port));

            set_non_blocking(con->sockfd);
            //disable rate limiting and TODO check that keep alive stops after connection close
            //enable_keepalive(con->sockfd);
            set_recv_window(con->sockfd);
            new_con(con->sockfd);

            //cant add EPOLLRDHUP as EPOLLEXCLUSIVE would then fail
            //instead check for a read of 0
            event.data.ptr = con;

            //we dont need to calloc the event its coppied.
            event.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLEXCLUSIVE;
            ensure(epoll_ctl(epollfds[epoll_pos], EPOLL_CTL_ADD, con->sockfd, &event) != -1);
            //round robin client addition
            epoll_pos = epoll_pos == total_threads ? 0 : epoll_pos + 1;
        }
    } else {
        wait(0);
    }
}
