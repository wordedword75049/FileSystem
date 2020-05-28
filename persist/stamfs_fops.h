
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
 * This function is used for reading the contents of a directory, and
 * passing it back to the user.
 */
int stamfs_readdir(struct file *filp, void *dirent, filldir_t filldir);

#endif /* STAMFS_FOPS_H */
