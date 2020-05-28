

#ifndef STAMFS_SUPER_H
#define STAMFS_SUPER_H

#include <linux/stddef.h>
#include <linux/fs.h>


/*
 * exported functions.
 */

struct super_block *stamfs_read_super (struct super_block *, void *, int);

#endif /* STAMFS_SUPER_H */
