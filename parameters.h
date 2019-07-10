/* SPDX-License-Identifier: BSD-2-Clause-FreeBSD */
/*******************************************************************************
FILE:		parameters.h
DESCRIPTION:	This file contains the configuration parameter parsing function

*******************************************************************************/

#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

void set_parameters(int argc, char *argv[]);

#endif /* __PARAMETERS_H__ */
