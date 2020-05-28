

#ifndef STAMFS_FOPS_H
#define STAMFS_FOPS_H

/*
 * Functions that handle VFS's "file-operations" for normal files and for
 * directories.
 */

/*
 * This struct contains the VFS file operations used to access the contents
 * of a directory.
 */
extern struct file_operations stamfs_dir_fops;

/*
 * This struct contains the VFS file operations used to access the contents
 * of a file.
 */
extern struct file_operations stamfs_file_fops;

/*
 * This function is used for reading the contents of a directory, and
 * passing it back to the user.
 */
int stamfs_readdir(struct file *filp, void *dirent, filldir_t filldir);

/*
 * This function is used for syncing the contents of a file to the disk
 * (i.e. forcing writing all dirty pages and buffer_head-s of this file
 * to the disk).
 */
int stamfs_sync_file(struct file *filp, struct dentry *dentry, int datasync);

#endif /* STAMFS_FOPS_H */
