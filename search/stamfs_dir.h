
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

#endif /* STAMFS_DIR_H */
