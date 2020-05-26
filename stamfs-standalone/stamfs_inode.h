/* This file is part of stamfs, a GNU/Linux kernel module that */
/* implements a file-system for accessing STAM devices.        */
/*                                                             */
/* Copyright (C) 2004 guy keren, choo@actcom.co.il             */
/* License: GNU General Public License                         */

/*
 *$Id$
 */

#ifndef STAMFS_INODE_H
#define STAMFS_INODE_H

#include <linux/stddef.h>
#include <linux/fs.h>


/* STAMFS meta-data to be attached to each VFS inode. */
struct stamfs_inode_meta_data {
        __u32  i_block_num;     /* block containing the inode.               */
        __u32  i_bi_block_num;  /* block containing the inode's block index. */
};

/* extract the STAMFS inode meta-data from a VFS inode. */
#define STAMFS_INODE_META(ino) ((struct stamfs_inode_meta_data *)((ino)->u.generic_ip))

/*
 * exported functions.
 */

/*
 * Given a VFS inode and the inode's block on disk, read the inode's contents
 * into memory. The inode number is supplied inside the VFS inode struct.
 * @return 0 on success, a negative error code on failure.
 */
int stamfs_inode_read_ino (struct inode *ino, unsigned long block_num);

/*
 * Update the on-disk copy of the given inode, based on the data in the given
 * VFS inode struct.
 * @param do_sync - 1 if we should perform the disk I/O now and wait for
 *                  its completion, 0 otherwise.
 * @return 0 on success, a negative error code on failure.
 */
int stamfs_inode_write_ino (struct inode *ino, int do_sync);

/* free the block index and the inode's block, as well as the inode number. */
int stamfs_inode_free_inode(struct inode *ino);

/*
 * free all data blocks of the given inode, making it refer to a
 * 0-length file.
 */
void stamfs_inode_truncate(struct inode *ino);

/*
 * Clear any dynamically-allocated resources used by us for the given
 * VFS inode struct.
 */
void stamfs_inode_clear (struct inode *ino);


#endif /* STAMFS_INODE_H */
