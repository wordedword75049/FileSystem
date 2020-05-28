

#ifndef STAMFS_SUPER_H
#define STAMFS_SUPER_H

#include <linux/stddef.h>
#include <linux/fs.h>


/*
 * exported functions.
 */

struct super_block *stamfs_read_super (struct super_block *, void *, int);

/*
 * Allocates a free block number.
 * returns 0 if no free numbers are available.
 */
int stamfs_alloc_block(struct super_block *sb);

/*
 * Frees a previously allocated block number.
 */
int stamfs_release_block(struct super_block *sb, int block_num);

/*
 * Allocates a free inode number, mapping it to the given block number.
 * returns 0 if no free numbers are available.
 */
ino_t stamfs_alloc_inode_num(struct super_block *sb, int block_num);

/*
 * Frees a previously allocated inode number.
 */
int stamfs_release_inode_num(struct super_block *sb, ino_t ino_num);

#endif /* STAMFS_SUPER_H */
