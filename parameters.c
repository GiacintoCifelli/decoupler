/* SPDX-License-Identifier: BSD-2-Clause-FreeBSD */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <getopt.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "decoupler.h"
#include "debug.h"
#include "parameters.h"

#define COMMANDLINE "ka [OPTIONS] listen_ip listen_port write_ip write_port"

static char *ka_description =
	"Usage: " COMMANDLINE "\n"
	"This program configure and drives up to two ofono modems in parallel\n"
	"It keep them with an active PDP context as much as possible\n"
	"It normally runs as a demon\n";

static struct option ka_options[] = {
	{"help",		no_argument,	0,	'h' },
	{"version",		no_argument,	0,	'v' },
	{"no-color",		no_argument,	0,	0 },
	{"force-color",		no_argument,	0,	0 },
	{"no-timestamp",	no_argument,	0,	0 },
	{0,			0,		0,	0 }
};
static char *ka_opt_descriptions[] = {
	"print this information and exit",
	"print version information and exit",
	"print trace output b&w",
	"print trace output always in color",
	"no timestamp in trace output",
	"",
};
static char *ka_refs = "\n"
	"License information is available under the source directory\n"
	"Further details in the docs subdirectory of the source tree\n"
	"Source repository: https://github.com/GiacintoCifelli/decoupler";

void set_parameters(int argc, char *argv[]) {
	setlocale(LC_ALL,"");
	for(;;) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "hv:", ka_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			if(strcmp(ka_options[option_index].name, "no-color")==0)
				check_color(-1);
			else if(strcmp(ka_options[option_index].name, "force-color")==0)
				check_color(1);
			else if(strcmp(ka_options[option_index].name, "no-timestamp")==0)
				dbg_use_timestamp=false;
			break;
		case 'h':
			puts(ka_description);
			for(int i=0;;i++) {
				if(!ka_options[i].name) break;
				if(ka_options[i].val)
					printf("  -%c, --%s\t%s\n", ka_options[i].val, ka_options[i].name, ka_opt_descriptions[i]);
				else
					printf("      --%s\t%s\n", ka_options[i].name, ka_opt_descriptions[i]);
			}
			puts(	"\nExit status:");
			printf(	" %d  if OK\n", EXIT_SUCCESS);
			printf(	" %d  if error\n", EXIT_FAILURE);
			puts(ka_refs);
			exit(EXIT_SUCCESS);
			break;
		case 'v':
			printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
			exit(EXIT_SUCCESS);
			break;
		case '?':
		case ':':
			break;
		default:
			printf("?? getopt returned character code 0%o ??\n", c);
		}
	}
	if(optind!=argc-4) {
		if(optind<argc-4)
			printf("too many parameters. Use:\n\t" LRED COMMANDLINE NOCOLOR "\n");
		if(optind>argc-4)
			printf("required parameters missing. Use:\n\t" LRED COMMANDLINE NOCOLOR "\n");
		exit(EXIT_FAILURE);
	}
	listen_addr = argv[optind++];
	listen_port = atoi(argv[optind++]);
	write_addr = argv[optind++];
	write_port = atoi(argv[optind++]);
}
