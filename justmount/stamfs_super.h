/* This file is part of stamfs, a GNU/Linux kernel module that */
/* implements a file-system for accessing STAM devices.        */
/*                                                             */
/* Copyright (C) 2004 guy keren, choo@actcom.co.il             */
/* License: GNU General Public License                         */

/*
 *$Id$
 */

#ifndef STAMFS_SUPER_H
#define STAMFS_SUPER_H

#include <linux/stddef.h>
#include <linux/fs.h>


/*
 * exported functions.
 */

struct super_block *stamfs_read_super (struct super_block *, void *, int);

#endif /* STAMFS_SUPER_H */
