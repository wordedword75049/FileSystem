/* This file is part of stamfs, a GNU/Linux kernel module that */
/* implements a file-system for accessing STAM devices.        */
/*                                                             */
/* Copyright (C) 2004 guy keren, choo@actcom.co.il             */
/* License: GNU General Public License                         */

/*
 *$Id$
 */

#ifndef STAMFS_IOPS_H
#define STAMFS_IOPS_H

/*
 * exported functions.
 */

#include <linux/fs.h>


extern struct inode_operations stamfs_dir_iops;

/*
 * given an inode, maps the given block offset to the given block number.
 * returns 0 on success or a negative error code on failure.
 */
int stamfs_inode_map_block_offset_to_number(struct inode *ino, int block_offset, int block_num);

/*
 * given an inode and a block offset, sets p_block_number to the block number
 * containing this block offset, or -1 if there is no block mapped at the
 * given offset.
 * returns 0 on success or a negative error code on failure.
 */
int stamfs_inode_block_offset_to_number(struct inode *ino, int block_offset, int *p_block_num);

/* the VFS inode-operation functions. */
struct dentry *stamfs_iop_lookup(struct inode *dir, struct dentry *dentry);
int stamfs_iop_unlink(struct inode *dir, struct dentry *dentry);
int stamfs_iop_mkdir(struct inode *dir, struct dentry *dentry, int mode);
int stamfs_iop_rmdir(struct inode *dir, struct dentry *dentry);

#endif /* STAMFS_IOPS_H */
