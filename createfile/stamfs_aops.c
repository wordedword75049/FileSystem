/* This file is part of stamfs, a GNU/Linux kernel module that */
/* implements a file-system for accessing STAM devices.        */
/*                                                             */
/* Copyright (C) 2004 guy keren, choo@actcom.co.il             */
/* License: GNU General Public License                         */

/*
 *$Id$
 */

#include "stamfs_aops.h"

/*
 * Data structures.
 */

/* empty - we don't support file data handling yet. */
struct address_space_operations stamfs_aops = {
};
