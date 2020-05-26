/* This file is part of stamfs, a GNU/Linux kernel module that */
/* implements a file-system for accessing STAM devices.        */
/*                                                             */
/* Copyright (C) 2004 guy keren, choo@actcom.co.il             */
/* License: GNU General Public License                         */

/*
 *$Id$
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "stamfs.h"

/* pre-allocated block numbers, for use by the root inode. */
#define ROOT_INODE_BLOCK_NUM (STAMFS_LAST_HARDCODED_BLOCK_NUM + 1)
#define ROOT_INODE_INDEX_BLOCK_NUM (ROOT_INODE_BLOCK_NUM + 1)
#define ROOT_INODE_FIRST_DATA_BLOCK_NUM (ROOT_INODE_INDEX_BLOCK_NUM + 1)
#define HIGHEST_USED_BLOCK_NUM ROOT_INODE_FIRST_DATA_BLOCK_NUM


/* print usage information and exit. */
void usage(const char* progname)
{
        fprintf(stderr, "Usage: %s [-f] <dev file|file>\n", progname);
        exit(1);
}

/* check that the given file path is valid, and points to a device file (unless
 * force == 1).
 */
int check_dev(const char* progname, const char *dev_path, int force, int* p_num_blocks)
{
        struct stat st;

        /* make sure the path exists. */
        if (stat(dev_path, &st) == -1) {
                int errnum = errno;
                fprintf(stderr,
                        "%s: cannot stat file '%s' - %s.\n",
                        progname, dev_path, strerror(errnum));
                return 0;
        }

        /* make sure we have write permission for this path. */
        if (access(dev_path, W_OK) == -1) {
                fprintf(stderr,
                        "%s: you have no write access to '%s'.\n",
                        progname, dev_path);
                return 0;
        }

        /* calculate the number of blocks in this file/device. */
        (*p_num_blocks) = st.st_size / STAMFS_BLOCK_SIZE;

        /* if we got the 'force' flag, make sure it's a normal file. */
        if (force) {
                if (!S_ISBLK(st.st_mode) && !S_ISREG(st.st_mode)) {
                        fprintf(stderr,
                                "%s: path '%s' is neither a file nor a "
                                "block device. cannot format.\n",
                                progname, dev_path);
                        return 0;
                }
                return 1;
        }

        if (!S_ISBLK(st.st_mode)) {
                fprintf(stderr,
                        "%s: path '%s' is not a block device. "
                        "cannot format.\n",
                        progname, dev_path);
                return 0;
        }

        return 1;
}

/* write the given data to the given logical block number.
 * returns 1 on success, 0 on failure.
 */
int write_stamfs_block(const char* progname, const char* dev_path,
                       int fd, const char* block_name,
                       int stamfs_block_num, char* data, int data_len)
{
        int rc;
        off_t seek_pos = stamfs_block_num * STAMFS_BLOCK_SIZE;

        /* we need to write into block #1. */
        rc = lseek(fd, seek_pos, SEEK_SET);
        if (rc == -1) {
                int errnum = errno;
                fprintf(stderr,
                        "%s: failed seeking into position %lu "
                        "of file '%s' - %s.\n",
                        progname,
                        seek_pos,
                        dev_path,
                        strerror(errnum));
                return 0;
        }
        if (rc != seek_pos) {
                fprintf(stderr,
                        "%s: failed seeking into position %lu of file '%s' - "
                        "lseek returned %d.\n",
                        progname,
                        seek_pos,
                        dev_path,
                        rc);
                return 0;
        }

        rc = write(fd, data, data_len);
        if (rc == -1) {
                int errnum = errno;
                fprintf(stderr,
                        "%s: failed writing '%s' block into '%s' - %s.\n",
                        progname,
                        block_name,
                        dev_path,
                        strerror(errnum));
                return 0;
        }
        if (rc != data_len) {
                fprintf(stderr,
                        "%s: got only partial write when writing '%s' block "
                        " into position %lu of file '%s'.\n",
                        progname,
                        block_name,
                        seek_pos,
                        dev_path);
                return 0;
        }

        return 1;
}

/* write the STAMFS super-block. */
int write_stamfs_super_block(const char* progname, const char* dev_path,
                             int fd, int num_blocks, int num_free_blocks)
{
        struct stamfs_super_block stamfs_sb;
        int rc;

        memset(&stamfs_sb, 0, sizeof(stamfs_sb));
        stamfs_sb.s_magic = STAMFS_SUPER_MAGIC;
        stamfs_sb.s_inodes_count = STAMFS_MAX_INODE_NUM;
        stamfs_sb.s_blocks_count = num_blocks;
        stamfs_sb.s_free_inodes_count = STAMFS_MAX_INODE_NUM - 1;
        stamfs_sb.s_free_blocks_count = num_free_blocks;
        stamfs_sb.s_free_list_block_num = STAMFS_FREE_LIST_BLOCK_NUM;
        stamfs_sb.s_highest_used_block_num = HIGHEST_USED_BLOCK_NUM;

        printf("%s: free blocks count: %d, blocks_count - %d\n",
               progname, num_free_blocks, num_blocks);

        /* we need to write into block #1. */
        rc = write_stamfs_block(progname, dev_path, fd, "super-block",
                                STAMFS_SUPER_BLOCK_NUM,
                                (char*)&stamfs_sb, sizeof(stamfs_sb));
        return rc;
}

/* write the inode index. */
int write_stamfs_inode_index(const char* progname, const char* dev_path, int fd)
{
        struct stamfs_inode_index stamfs_ii;
        int rc;

        /* everything should be zero, except for the root inode. */
        memset((char*)&stamfs_ii, 0, sizeof(stamfs_ii));
        stamfs_ii.index[STAMFS_ROOT_INODE_NUM-1] = ROOT_INODE_BLOCK_NUM;

        /* we need to write into block #2. */
        rc = write_stamfs_block(progname, dev_path, fd, "inode-index",
                                STAMFS_INODES_BLOCK_NUM,
                                (char*)&stamfs_ii, sizeof(stamfs_ii));
        return rc;
}

/* write the (empty) free-list block. */
int write_stamfs_free_list_block(const char* progname, const char* dev_path,
                                 int fd)
{
        char buf[STAMFS_BLOCK_SIZE];
        int rc;

        memset(buf, 0, sizeof(buf));

        /* we need to write into block #STAMFS_FREE_LIST_BLOCK_NUM. */
        rc = write_stamfs_block(progname, dev_path, fd, "free-list",
                                STAMFS_FREE_LIST_BLOCK_NUM,
                                (char*)buf, sizeof(buf));
        return rc;
}

/* write the first (and only) data block of the root directory (i.e. the root
 * inode).
 */
int write_stamfs_root_inode_first_data_block(const char* progname,
                                             const char* dev_path, int fd)
{
        struct stamfs_dir_rec stamfs_dr;
        int rc;

        memset((char*)&stamfs_dr, 0, sizeof(stamfs_dr));
        stamfs_dr.dr_ino = 0;

        /* we need to write into block #ROOT_INODE_FIRST_DATA_BLOCK_NUM. */
        rc = write_stamfs_block(progname, dev_path, fd,
                                "root inode first data block",
                                ROOT_INODE_FIRST_DATA_BLOCK_NUM,
                                (char*)&stamfs_dr, sizeof(stamfs_dr));
        return rc;
}

/* write the block index of the root inode. */
int write_stamfs_root_inode_block_index(const char* progname,
                                        const char* dev_path, int fd)
{
        struct stamfs_inode_block_index stamfs_root_bi;
        int rc;

        memset((char*)&stamfs_root_bi, 0, sizeof(stamfs_root_bi));
        stamfs_root_bi.index[0] = ROOT_INODE_FIRST_DATA_BLOCK_NUM;

        /* we need to write into block #STAMFS_ROOT_INODE_BLOCK_NUM. */
        rc = write_stamfs_block(progname, dev_path, fd,
                                "root inode block index",
                                ROOT_INODE_INDEX_BLOCK_NUM,
                                (char*)&stamfs_root_bi, sizeof(stamfs_root_bi));
        return rc;
}

/* write the root inode. */
int write_stamfs_root_inode(const char* progname, const char* dev_path, int fd)
{
        struct stamfs_inode stamfs_root_ino;
        int rc;

        memset((char*)&stamfs_root_ino, 0, sizeof(stamfs_root_ino));
        /* permissions - 0x40755 */
        stamfs_root_ino.i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
        stamfs_root_ino.i_uid = 0;
        stamfs_root_ino.i_gid = 0;
        stamfs_root_ino.i_size = STAMFS_BLOCK_SIZE;
        stamfs_root_ino.i_atime = 0;
        stamfs_root_ino.i_mtime = 0;
        stamfs_root_ino.i_ctime = 0;
        stamfs_root_ino.i_num_blocks = 1;
        stamfs_root_ino.i_num_links = 1;
        stamfs_root_ino.i_index_block = ROOT_INODE_INDEX_BLOCK_NUM;

        /* we need to write into block #ROOT_INODE_BLOCK_NUM. */
        rc = write_stamfs_block(progname, dev_path, fd, "inode-index",
                                ROOT_INODE_BLOCK_NUM,
                                (char*)&stamfs_root_ino,
                                sizeof(stamfs_root_ino));
        if (!rc)
                return 0;

        if (!write_stamfs_root_inode_block_index(progname, dev_path, fd))
                return 0;

        return write_stamfs_root_inode_first_data_block(progname, dev_path, fd);
}

/* create the STAMFS file-system structure. */
int mkstamfs(const char* progname, const char* dev_path,
             int num_blocks, int num_free_blocks)
{
        int fd = open(dev_path, O_WRONLY | O_EXCL);

        if (fd == -1) {
                int errnum = errno;
                fprintf(stderr,
                        "%s: failed opening file '%s' for writing - %s'n",
                        progname,
                        dev_path,
                        strerror(errnum));
                return 0;
        }

        if (!write_stamfs_super_block(progname, dev_path, fd, num_blocks,
                                      num_free_blocks)) {
                close(fd);
                return 0;
        }

        if (!write_stamfs_inode_index(progname, dev_path, fd)) {
                close(fd);
                return 0;
        }

        if (!write_stamfs_free_list_block(progname, dev_path, fd)) {
                close(fd);
                return 0;
        }

        if (!write_stamfs_root_inode(progname, dev_path, fd)) {
                close(fd);
                return 0;
        }

        if (close(fd) == -1) {
                int errnum = errno;
                fprintf(stderr,
                        "%s: error while closing file '%s' - %s'n",
                        progname,
                        dev_path,
                        strerror(errnum));
                return 0;
        }

        return 1;
}

int main(int argc, char *argv[])
{
        const char *dev_path = NULL;
        int force = 0;
        int num_blocks = 0;
        int free_blocks = 0;
        const char* progname = argv[0];

        if (argc < 2)
                usage(progname);

        /* parse command-line options. */
        if (strcmp(argv[1], "-f") == 0) {
            force = 1;
            argv++;
            argc--;
        }

        if (argc < 2)
                usage(progname);

        dev_path = argv[1];

        /* make necessary checks - the path exists, and either points to
         * a device file, or force==1. */
        if (!check_dev(progname, dev_path, force, &num_blocks))
                exit(1);
        free_blocks = num_blocks - (HIGHEST_USED_BLOCK_NUM + 1);

        /* create the file system. */
        if (!mkstamfs(progname, dev_path, num_blocks, free_blocks))
                exit(1);

        return 0;
}
