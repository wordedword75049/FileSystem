

#ifndef STAMFS_DIR_H
#define STAMFS_DIR_H

/*
 * Functions that deal with directories contents - as stored on a STAMFS disk.
 */

#include <linux/fs.h>

/*
 * return the block number of the (first) data block of the given directory.
 * returns a negative error code, in case of an error, 0 on success.
 */
int stamfs_dir_get_data_block_num(struct inode *dir, int *p_data_block_num);

/*
 * Given a directory's inode and a file-name, returns the inode number of
 * this file.
 * @return 0 on success, a negative error value in case of an error, or if
 *         there is no file with that name in this directory.
 */
int stamfs_dir_get_file_by_name(struct inode *dir, const char *name,
                                int namelen, ino_t* p_ino_num);

/*
 * Given a directory's inode, create the data part of this directory on disk,
 * as an empty directory.
 * Note: we assume that the inode itself was already created.
 */
int stamfs_inode_make_empty_dir(struct inode *dir);

/*
 * Given a directory's inode and a child inode, add this child inode as an
 * entry in the directory's data.
 * @return 0 on success, a negatiev error code on failure.
 */
int stamfs_dir_add_link(struct inode *parent_dir, struct inode *child,
                        const char *name, int namelen);

/*
 * Given a directory's inode and a child inode, remove this child inode from
 * the directory's list-of-entries.
 * @return 0 on success, a negative error code on failure.
 */
int stamfs_dir_del_link(struct inode *parent_dir, struct inode *child,
                        const char *name, int namelen);

/*
 * Given a directory's inode, check if this directory is empty.
 * returns 0 if it is, 1 if it's not, a negative value in case of an error
 * (e.g. I/O error).
 */
int stamfs_dir_is_empty(struct inode *dir);

#endif /* STAMFS_DIR_H */
