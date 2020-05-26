/* This file is part of stamfs, a GNU/Linux kernel module that */
/* implements a file-system for accessing STAM devices.        */
/*                                                             */
/* Copyright (C) 2004 guy keren, choo@actcom.co.il             */
/* License: GNU General Public License                         */

/*
 *$Id$
 */

#ifndef STAMFS_AOPS_H
#define STAMFS_AOPS_H

/*
 * Functions dealing with address-space mapping - used by the page-cache.
 */

#include <linux/mm.h>

/*
 * This struct contains the address-space operations used to read and write
 * data pages (which are contents of files in STAMFS).
 */
extern struct address_space_operations stamfs_aops;

#endif /* STAMFS_AOPS_H */
