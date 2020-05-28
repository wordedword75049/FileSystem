

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

#endif /* STAMFS_IOPS_H */
