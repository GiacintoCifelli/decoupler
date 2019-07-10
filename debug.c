/* SPDX-License-Identifier: BSD-2-Clause-FreeBSD */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"

const char *getdatetime()
{
	static char dt[32];
	time_t o=time(NULL);
	strftime(dt,sizeof(dt),"[%F, %T] ",localtime(&o));
	return (const char*)dt;
}

const char *getdatetimecompact()
{
	static char dt[32];
	time_t o=time(NULL);
	strftime(dt,sizeof(dt),"%F-%T",localtime(&o));
	return (const char*)dt;
}

const char *getdatetimeshort()
{
	static char dt[32];
	time_t o=time(NULL);
	strftime(dt,sizeof(dt),"%Y%m%d%H%M%S",localtime(&o));
	return (const char*)dt;
}

char* escape_string(const char *str) {
	const char *s = str;
	int len = 0;
	while(*s) {
		switch(*s) {
		case '\r':
		case '\n':
		case '\t':
			len += 2;
			break;
		default:
			len++;
			break;
		}
		s++;
	}
	char *out = calloc(len+1, sizeof(char));
	char *o = out;
	s = str;
	while(*s) {
		switch(*s) {
		case '\r':
			*o++ = '\\';
			*o++ = 'r';
			break;
		case '\n':
			*o++ = '\\';
			*o++ = 'n';
			break;
		case '\t':
			*o++ = '\\';
			*o++ = 't';
			break;
		default:
			*o++ = *s;
			break;
		}
		s++;
	}
	return out;
}

void print_escaped(const char *s) {
	char *se = escape_string(s);
	fprintf(stderr, "'%s'",se);
	free(se);
}

void print_trace() {
	char pid_buf[30];
	char name_buf[512];
	int child_pid;
	sprintf(pid_buf, "%d", getpid());
	name_buf[readlink("/proc/self/exe", name_buf, 511)]=0;
	child_pid = fork();
	if(!child_pid) {
		dup2(2,1); /* redirect output to stderr */
		fprintf(stderr,"stack trace for %s pid=%s\n",name_buf,pid_buf);
		execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);
		abort(); /* If gdb failed to start */
	} else {
		waitpid(child_pid,NULL,0);
	}
}

bool dbg_color = true;
bool dbg_use_timestamp = true;
bool dbg_print_prio = false;

char *prios[] = {
	PRIO_EMERG,
	PRIO_ALERT,
	PRIO_CRIT,
	PRIO_ERROR,
	PRIO_WARNING,
	PRIO_NOTICE,
	PRIO_INFO,
	PRIO_DEBUG,
};

int logfacility = use_stderr;
int minlogpriority = LOG_DEBUG;
char log_buffer[LOGBUFFER_SIZE];

void check_color(enum force_color force) {
	if(force==force_with_color)
		dbg_color = true;
	else if(force==force_without_color)
		dbg_color = false;
	else if(isatty(STDOUT_FILENO))
		dbg_color = true;
	else
		dbg_color = false;
}

void nsleep(int64_t nanoseconds) {
	struct timespec ts = { .tv_sec = nanoseconds/ONE_SECOND_NS, .tv_nsec = nanoseconds%ONE_SECOND_NS };
	nanosleep(&ts, NULL);
}
