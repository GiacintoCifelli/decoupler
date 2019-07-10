/* SPDX-License-Identifier: BSD-2-Clause-FreeBSD */

#ifndef __DECOUPLER_H__
#define __DECOUPLER_H__

#define _GNU_SOURCE

typedef enum {
	serve_first, /* serve the first connected client and reject further connections */
	serve_last, /* consider the connection dropped unexpectedly and close the previous socket */
	serve_all /* RFU */
} serve_t;

extern const char *listen_addr;
extern int listen_port;
extern const char *write_addr;
extern int write_port;

extern serve_t serve_policy;

#endif /* __DECOUPLER_H__ */
