
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

/*
 * Given an inode, find the block number to which a given block offset is
 * mapped.
 * returns 0 on success or a negative error code on failure.
 */
int stamfs_get_block(struct inode *ino, long block_offset, struct buffer_head *bh_result, int create);

#endif /* STAMFS_AOPS_H */
