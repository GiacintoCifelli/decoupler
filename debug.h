/* SPDX-License-Identifier: BSD-2-Clause-FreeBSD */
/*******************************************************************************
FILE:		debug.h
DESCRIPTION:	This file contains debug facilities definitions and macros.

*******************************************************************************/

#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <termios.h>

#include "version.h"

#define RED		"\033[00;31m"
#define GREEN		"\033[00;32m"
#define YELLOW		"\033[00;33m"
#define BLUE		"\033[00;34m"
#define PURPLE		"\033[00;35m"
#define CYAN		"\033[00;36m"
#define LIGHTGRAY	"\033[00;37m"

#define LRED		"\033[01;31m"
#define LGREEN		"\033[01;32m"
#define LYELLOW		"\033[01;33m"
#define LBLUE		"\033[01;34m"
#define LPURPLE		"\033[01;35m"
#define LCYAN		"\033[01;36m"
#define WHITE		"\033[01;37m"

#define NOCOLOR		"\033[00m"

const char *getdatetime();
const char *getdatetimecompact();
const char *getdatetimeshort();

char* escape_string(const char *str);
void print_escaped(const char *s);
void print_trace();

extern bool dbg_color;
extern bool dbg_use_timestamp;
extern bool dbg_print_prio;

enum force_color {
	force_with_color	= 1,
	no_force_color		= 0,
	force_without_color	= -1,
};

void check_color(enum force_color force);

#define PRIO_EMERG	"<0>"
#define PRIO_ALERT	"<1>"
#define PRIO_CRIT	"<2>"
#define PRIO_ERROR	"<3>"
#define PRIO_WARNING	"<4>"
#define PRIO_NOTICE	"<5>"
#define PRIO_INFO	"<6>"
#define PRIO_DEBUG	"<7>"

/* the numeric constants are defined in syslog.h as LOG_xxx */

extern char* prios[];

enum logfacilities {
	init_use_syslog=2,
	use_syslog=1,
	use_stdout=0,
	use_stderr=3
};
extern int logfacility; /* 0: use logfile, 2: init and use syslog, 1: use syslog*/
extern int minlogpriority;
#define LOGBUFFER_SIZE		(2048)
extern char log_buffer[LOGBUFFER_SIZE];

#define printlog(priority, format, args...) ({\
	if(priority<=minlogpriority) { \
		if(logfacility==init_use_syslog) { /* init and print in syslog */ \
			openlog(PROGRAM_NAME, LOG_CONS | LOG_NDELAY | LOG_NOWAIT, LOG_LOCAL0); \
			atexit(closelog); \
			logfacility = use_syslog; \
		} \
		if(logfacility==use_syslog) /* print in syslog */ \
			syslog(priority, "%s" format, log_buffer, ##args); \
		if(logfacility==use_stderr || logfacility==use_stdout) { \
			FILE *logfile = (logfacility==use_stderr?stderr:stdout); \
			fprintf(logfile, "%s" format, log_buffer, ##args); \
		} \
	} \
})

#define hdbg(priority) ({ \
	if(dbg_color) \
		snprintf(log_buffer, sizeof(log_buffer), "%s" CYAN"%s"NOCOLOR"%s" GREEN"%s:%d"NOCOLOR"-"YELLOW"%s()"NOCOLOR, \
			dbg_print_prio?prios[priority]:"", \
			dbg_use_timestamp?getdatetimecompact():"", dbg_use_timestamp?"-":"", \
			__FILE__, __LINE__, __FUNCTION__); \
	else \
		snprintf(log_buffer, sizeof(log_buffer), "%s" "%s%s" "%s:%d""-""%s()", \
			dbg_print_prio?prios[priority]:"", \
			dbg_use_timestamp?getdatetimecompact():"", dbg_use_timestamp?"-":"", \
			__FILE__, __LINE__, __FUNCTION__); \
})

#define dbg(fmt, arg...) ({ \
	hdbg(LOG_DEBUG); \
	printlog(LOG_DEBUG, " " fmt "\n", ## arg); \
})

#define err(fmt, arg...) ({ \
	hdbg(LOG_INFO); \
	printlog(LOG_INFO, " " fmt "\n", ## arg); \
})

#define reterr(ret, fmt, arg...) ({ \
	hdbg(LOG_INFO); \
	printlog(LOG_INFO, " " fmt "\n", ## arg); \
	return ret; \
})

#define checkptr(p) ({ \
	if(!p) err("pointer null: %s", #p); \
	p; \
})

#define ptr(p) ({ \
	if(!p) { \
		err("pointer null: %s", #p); \
		return; \
	} \
	p; \
})

#define retptr(ret, p) ({ \
	if(!p) reterr(ret, "pointer null: %s", #p); \
	p; \
})

#define FREE(a) ({dbg("---free:"#a"=%p",a) free(a);})
#define CALLOC(a,b) ({void *c=calloc(a,b); dbg("---calloc:"#a","#b"=%p",c) c;})

#define print_opaque(f, var) { \
	unsigned char *p = (unsigned char*)(void*)(&var); \
	char *buf = alloca(sizeof(var)*2+1); \
	for (size_t i=0; i<sizeof(var); i++) \
		sprintf(buf, "%02x", (unsigned)(p[i])); \
	fprintf(f, "%s", buf); \
}

#define ONE_SECOND_NS	(1000*1000*1000)
#define ONE_MS_NS	(1000*1000)
#define ONE_uS_NS	(1000)
void nsleep(int64_t nanoseconds);

#endif /* __DEBUG_H__ */
