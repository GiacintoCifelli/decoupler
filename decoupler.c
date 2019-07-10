/* SPDX-License-Identifier: BSD-2-Clause-FreeBSD */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "decoupler.h"
#include "debug.h"
#include "parameters.h"
#include "version.h"

#define MAXMSG			(1024)
#define MAX_EPOLL_EVENTS	(64)

const char *listen_addr = "127.0.0.1";
int listen_port = 5555;
const char *write_addr = "127.0.0.1";
int write_port = 5000;
serve_t serve_policy=serve_last;

static bool connected = false;
static int epfd, serverfd;
static struct epoll_event servere;

static int make_sockets() {
	int listenfd;
	struct sockaddr_in name;
	listenfd=socket(PF_INET, SOCK_STREAM, 0); /* Create the socket. */
	if(listenfd<0) {
		perror ("socket");
		exit (EXIT_FAILURE);
	}
	name.sin_family = AF_INET; /* Give the socket a name. */
	name.sin_port = htons (listen_port);
	name.sin_addr.s_addr = htonl (INADDR_ANY);
	if (bind (listenfd, (struct sockaddr *) &name, sizeof (name)) < 0) {
		perror ("bind");
		exit (EXIT_FAILURE);
	}
	struct linger a;
	a.l_onoff=0;
	a.l_linger=0;
	setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &a, sizeof(a));
	return listenfd;
}

static void make_socketc() {
	if(connected)
		return;
	struct sockaddr_in serv_addr = {0};
	serverfd=socket(PF_INET, SOCK_STREAM, 0); /* Create the socket. */
	if(serverfd<0) {
		perror ("socket");
		exit (EXIT_FAILURE);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(write_port);
	if(inet_pton(AF_INET, write_addr, &serv_addr.sin_addr)<=0)
		return;
	if(connect(serverfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
		return;
	struct linger a;
	a.l_onoff=0;
	a.l_linger=0;
	setsockopt(serverfd, SOL_SOCKET, SO_LINGER, &a, sizeof(a));
	servere.events=EPOLLERR|EPOLLRDHUP|EPOLLHUP;
	servere.data.fd=serverfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, serverfd, &servere);
	connected=true;
}

static void resend(char *buffer, int nbytes) {
	make_socketc();
	if(!connected)
		return; /* and buffer discarded */
	int ret;
	bool finished=false;
	while(!finished) {
		ret=write(serverfd,buffer, nbytes);
		if(ret<=0) {
			close(serverfd);
			connected=false;
			finished=true;
		} else if(ret<nbytes)
			nbytes-=ret; /* and not finished */
		else
			finished=true;
	}
}

static int read_from_client (int filedes) {
	char buffer[MAXMSG] = {0};
	int nbytes;
	nbytes = read (filedes, buffer, MAXMSG);
	if (nbytes <= 0)
		return -1;
	resend(buffer, nbytes);
	return 0;
}

int main(int argc, char *argv[]) {
	set_parameters(argc, argv);
	dbg("%s %s", PROGRAM_NAME, PROGRAM_VERSION);
	int sigfd, listenfd, connfd=-1, clientfd=-1;
	struct epoll_event sige, listene, conne, cliente;
	epfd = epoll_create1(0);
	if(epfd==-1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	if(sigprocmask(SIG_BLOCK, &mask, NULL) == -1) { /* Block signals so that they aren't handled according to their default dispositions */
		perror("sigprocmask");
		exit(EXIT_FAILURE);
	}
	sigfd = signalfd(-1, &mask, 0);
	if(sigfd==-1) {
		perror("signalfd");
		exit(EXIT_FAILURE);
	}
	sige.events = EPOLLIN;
	sige.data.fd = sigfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &sige);
	listenfd=make_sockets(); /* Create the socket and set it up to accept connections. */
	if(listen(listenfd, 1) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	listene.events = EPOLLIN;
	listene.data.fd = listenfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &listene);

	while(true) {
		struct epoll_event events[MAX_EPOLL_EVENTS];
		int num_ready = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 100 /* timeout = 100ms */);
		if(num_ready==-1) {
			perror("select");
			exit(EXIT_FAILURE);
		}
		if(num_ready==0) /* timeout processing */
			make_socketc();
		for(int i=0;i<num_ready;i++) { /* first pass: look for errors */
			if((events[i].events & (EPOLLERR|EPOLLRDHUP|EPOLLHUP)) != 0) {
				if(events[i].data.fd == serverfd) {
					epoll_ctl(epfd, EPOLL_CTL_DEL, serverfd, &servere);
					close(serverfd);
					serverfd=-1; // todo: move in a disconnect_server() function, for all detections. remove redundant connected flag (can test serverfd!=-1)
					connected=false;
				}
				// todo: detect here also if client disconnects
			}
		}
		for(int i=0;i<num_ready;i++) { /* second pass: process regular I/O */
			if(events[i].events & EPOLLIN) {
				if(events[i].data.fd == sigfd) {
					close(serverfd);
					close(connfd);
					close(listenfd);
					exit(EXIT_SUCCESS);
				} else if(events[i].data.fd == listenfd) {
					int new;
					struct sockaddr_in clientname;
					unsigned int size = sizeof(clientname);
					new = accept(listenfd, (struct sockaddr *)&clientname, &size);
					if(new<0) {
						perror("accept");
						continue;
					}
					if(connfd<0) {
						fprintf(stderr, "Server: connect from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));
						connfd=new;
						conne.events=EPOLLIN;
						conne.data.fd=connfd;
						epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &conne);
					} else {
						if(serve_policy==serve_last) {
							fprintf(stderr, "Server: re-connect from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));
							epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, &conne);
							close(connfd);
							connfd=new;
							conne.events=EPOLLIN;
							conne.data.fd=connfd;
							epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &conne);
						} else { /*if(serve_policy==serve_first)*/
							fprintf(stderr, "Server: reject connection from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));
							close(new);
						}
					}
				} else if(events[i].data.fd == connfd) { /* data on connected socket */
					if(read_from_client(connfd)<0) {
						epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, &conne);
						close(connfd);
						connfd=-1;
					}
				}
			}
		}
	};
	return EXIT_SUCCESS;
}
