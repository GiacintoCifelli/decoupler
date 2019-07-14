/* SPDX-License-Identifier: BSD-2-Clause-FreeBSD */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <locale.h>
#include <netdb.h>
#include <poll.h>
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
serve_t serve_policy=serve_last; // todo: option for selecting serve_first instead

static int epfd, sigfd, writefd=-1, listenfd=-1, connfd=-1;
static struct epoll_event writee, listene, sige, conne;

void set_nonblock(int fd)
{
	int fl;
	int x;
	fl = fcntl(fd, F_GETFL, 0);
	if(fl<0) return;
	fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static void create_listener() {
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	listenfd=socket(PF_INET, SOCK_STREAM, 0);
	exit_on_error(listenfd, "socket");
	set_nonblock(listenfd);
	sa.sin_family=AF_INET;
	sa.sin_addr.s_addr=inet_addr(listen_addr); /* sa.sin_addr.s_addr=htonl(INADDR_ANY); - RFU to listen on all interfaces */
	sa.sin_port=htons(listen_port);
	int ret=bind(listenfd, (struct sockaddr*)&sa, sizeof(sa));
	exit_on_error(ret, "bind");
	uint32_t on=1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	struct linger a;
	a.l_onoff=0;
	a.l_linger=0;
	setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &a, sizeof(a));
	ret=listen(listenfd, 1);
	exit_on_error(ret, "listen");
	listene.events = EPOLLIN;
	listene.data.fd = listenfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &listene);
}

static void create_client() {
	if(writefd>=0)
		return;
	struct sockaddr_in serv_addr = {0};
	writefd=socket(PF_INET, SOCK_STREAM, 0); /* Create the socket. */
	exit_on_error(writefd,"socket");
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(write_port);
	if(inet_pton(AF_INET, write_addr, &serv_addr.sin_addr)<=0) {
		close(writefd);
		writefd=-1;
		return;
	}
	if(connect(writefd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0) {
		close(writefd);
		writefd=-1;
		return;
	}
	uint32_t on=1;
	setsockopt(writefd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	struct linger a;
	a.l_onoff=0;
	a.l_linger=0;
	setsockopt(writefd, SOL_SOCKET, SO_LINGER, &a, sizeof(a));
	writee.events=EPOLLERR|EPOLLRDHUP|EPOLLHUP|EPOLLIN;
	writee.data.fd=writefd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, writefd, &writee);
}

static void disconnect_server() {
	epoll_ctl(epfd, EPOLL_CTL_DEL, writefd, &writee);
	close(writefd);
	writefd=-1;
}

int write_fd(int fd, void *buf, size_t size) {
	size_t written = 0;
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = EPOLLOUT | EPOLLERR | EPOLLRDHUP | EPOLLHUP;
	while(written<size) {
		int pollret = poll(&pfd, 1, 1); /* block max 1ms, just to check errors */
		if(pollret<0) {
			return -1;
		}  else if (pollret>0) {
			if(pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
				return -1;
			} else
				written += write(fd, buf+written, size-written);
		}
	}
	return written;
}

static void resend(int dest, char *buffer, int nbytes) {
	create_client();
	if(dest<0) return; /* buffer discarded */
	int ret = write_fd(dest, buffer, nbytes);
	if(ret<0 && dest==writefd)
		disconnect_server();
}

static int read_from_client(int source, int dest) {
	char buffer[MAXMSG] = {0};
	int nbytes;
	nbytes = read (source, buffer, MAXMSG);
	if (nbytes <= 0)
		return -1;
	resend(dest, buffer, nbytes);
	return 0;
}

void accept_connection() {
	int new;
	struct sockaddr_in clientname;
	unsigned int size = sizeof(clientname);
	new = accept(listenfd, (struct sockaddr *)&clientname, &size);
	if(new<0) return;
	if(connfd<0) {
		dbg("Server: connect from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));
		connfd=new;
		conne.events=EPOLLIN;
		conne.data.fd=connfd;
		epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &conne);
	} else {
		if(serve_policy==serve_last) {
			dbg("Server: re-connect from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));
			epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, &conne);
			close(connfd);
			connfd=new;
			conne.events=EPOLLIN;
			conne.data.fd=connfd;
			epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &conne);
		} else { /*if(serve_policy==serve_first)*/
			dbg("Server: reject connection from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));
			close(new);
		}
	}
}

void get_signals() {
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	int ret=sigprocmask(SIG_BLOCK, &mask, NULL);
	exit_on_error(ret, "sigprocmask");
	sigfd = signalfd(-1, &mask, 0);
	exit_on_error(sigfd, "signalfd");
	sige.events = EPOLLIN;
	sige.data.fd = sigfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &sige);
}

int main(int argc, char *argv[]) {
	set_parameters(argc, argv);
	dbg("%s %s", PROGRAM_NAME, PROGRAM_VERSION);
	epfd = epoll_create1(0);
	exit_on_error(epfd,"epoll_create1");
	get_signals();
	create_listener();
	while(true) {
		struct epoll_event events[MAX_EPOLL_EVENTS];
		int num_ready = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 100 /* timeout = 100ms */);
		exit_on_error(num_ready, "epoll_wait");
		if(num_ready==0) /* timeout */
			create_client();
		for(int i=0;i<num_ready;i++) { /* first pass: look for errors */
			if((events[i].events & (EPOLLERR|EPOLLRDHUP|EPOLLHUP)) != 0) {
				if(events[i].data.fd == writefd) {
					disconnect_server();
				} else if(events[i].data.fd == listenfd) {
					dbg("listener socket error/disconnection");
					exit(EXIT_FAILURE);
				} else if(events[i].data.fd == connfd) {
					dbg("Server: client disconnected.");
					epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, &conne);
					close(connfd);
					connfd=-1;
				}
			}
		}
		for(int i=0;i<num_ready;i++) { /* second pass: process regular I/O */
			if(events[i].events & EPOLLIN) {
				if(events[i].data.fd == sigfd) {
					close(writefd);
					close(connfd);
					close(listenfd);
					exit(EXIT_SUCCESS);
				} else if(events[i].data.fd == listenfd) {
					accept_connection();
				} else if(events[i].data.fd == connfd) { /* data on conn socket */
					if(read_from_client(connfd, writefd)<0) {
						epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, &conne);
						close(connfd);
						connfd=-1;
					}
				} else if(events[i].data.fd == writefd) { /* data on conn socket */
					read_from_client(writefd, connfd);
				}
			}
		}
	};
	return EXIT_SUCCESS;
}
