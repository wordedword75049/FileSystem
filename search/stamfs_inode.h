

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
 * Clear any dynamically-allocated resources used by us for the given
 * VFS inode struct.
 */
void stamfs_inode_clear (struct inode *ino);


#endif /* STAMFS_INODE_H */
