

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "stamfs.h"

/* globals. */
struct stamfs_super_block stamfs_sb;
struct stamfs_inode_index stamfs_ii;

void usage(const char* progname)
{
        fprintf(stderr, "Usage: %s [-f] <dev file|file>\n", progname);
        exit(1);
}

int check_dev(const char* progname, const char *dev_path, int force)
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

        /* make sure we have read permission for this path. */
        if (access(dev_path, R_OK) == -1) {
                fprintf(stderr,
                        "%s: you have no read access to '%s'.\n",
                        progname, dev_path);
                return 0;
        }

        /* if we got the 'force' flag, make sure it's a normal file. */
        if (force) {
                if (!S_ISBLK(st.st_mode) && !S_ISREG(st.st_mode)) {
                        fprintf(stderr,
                                "%s: path '%s' is neither a file nor a "
                                "block device. cannot read.\n",
                                progname, dev_path);
                        return 0;
                }
                return 1;
        }

        if (!S_ISBLK(st.st_mode)) {
                fprintf(stderr,
                        "%s: path '%s' is not a block device. cannot read.\n",
                        progname, dev_path);
                return 0;
        }

        return 1;
}

/* read data in a given length from the given logical block.
 * return 1 on success, 0 on failure.
 */
int read_stamfs_block(const char* progname, const char* dev_path, int fd,
                      const char* block_name, int stamfs_block_num,
                      char* data, int data_len)
{
        int rc;
        off_t seek_pos = stamfs_block_num * STAMFS_BLOCK_SIZE;

        rc = lseek(fd, seek_pos, SEEK_SET);
        if (rc == -1) {
                int errnum = errno;
                fprintf(stderr,
                        "%s: failed seeking into position %ld of "
                        "file '%s' - %s.\n",
                        progname,
                        seek_pos,
                        dev_path,
                        strerror(errnum));
                return 0;
        }
        if (rc != seek_pos) {
                fprintf(stderr,
                        "%s: failed seeking into position %ld of file '%s' "
                        "- lseek returned %d.\n",
                        progname,
                        seek_pos,
                        dev_path,
                        rc);
                return 0;
        }

        rc = read(fd, data, data_len);
        if (rc == -1) {
                int errnum = errno;
                fprintf(stderr,
                        "%s: failed reading block '%s' from '%s' - %s.\n",
                        progname,
                        block_name,
                        dev_path,
                        strerror(errnum));
                return 0;
        }
        if (rc != data_len) {
                fprintf(stderr,
                        "%s: got only partial read when reading block '%s' "
                        "from '%s'.\n",
                        progname,
                        block_name,
                        dev_path);
                return 0;
        }

        return 1;
}

int read_stamfs_super_block(const char* progname, const char* dev_path, int fd)
{
        int rc;

        /* we need to read from block #1. */
        rc = read_stamfs_block(progname, dev_path, fd,
                               "super-block", STAMFS_SUPER_BLOCK_NUM,
                               (char*)&stamfs_sb, sizeof(stamfs_sb));
        if (!rc)
                return 0;

        printf("Super-block:\n");
        printf("    magic: 0x%x\n", stamfs_sb.s_magic);
        printf("    inodes_count: %d\n", stamfs_sb.s_inodes_count);
        printf("    blocks_count: %d\n", stamfs_sb.s_blocks_count);
        printf("    free_inodes_count: %d\n", stamfs_sb.s_free_inodes_count);
        printf("    free_blocks_count: %d\n", stamfs_sb.s_free_blocks_count);
        printf("    free_list_block_num: %d\n",
               stamfs_sb.s_free_list_block_num);
        printf("    highest_used_block_num: %d\n",
               stamfs_sb.s_highest_used_block_num);

        return 1;
}

int read_stamfs_inode_index(const char* progname, const char* dev_path, int fd)
{
        int rc;
        int i;

        /* we need to read from block #2. */
        rc = read_stamfs_block(progname, dev_path, fd,
                               "inode-index", STAMFS_INODES_BLOCK_NUM,
                               (char*)&stamfs_ii, sizeof(stamfs_ii));
        if (!rc)
                return 0;

        printf("Inode-index (inode# -> block#):\n");
        for (i=0; i < STAMFS_MAX_INODE_NUM-1; ++i) {
                if (stamfs_ii.index[i] != 0)
                        printf("    %04d -> %06d\n", i+1, stamfs_ii.index[i]);

        }

        return 1;
}

int read_stamfs_free_list_block(const char* progname, const char* dev_path,
                                int fd)
{
        struct stamfs_free_list_index stamfs_fl;
        int rc;
        int i;

        /* we need to read from block #STAMFS_FREE_LIST_BLOCK_NUM. */
        rc = read_stamfs_block(progname, dev_path, fd,
                               "free-list", STAMFS_FREE_LIST_BLOCK_NUM,
                               (char*)&stamfs_fl, sizeof(stamfs_fl));
        if (!rc)
                return 0;

        printf("Free-blocks-list:\n");
        for (i=0; i < STAMFS_MAX_BLOCK_NUMS_PER_BLOCK-1; ++i) {
                if (stamfs_fl.index[i] == 0)
                        break;
                printf("    %d: %06d\n", i, stamfs_fl.index[i]);
        }

        return 1;
}


/* forward decleration. */
int read_stamfs_inode(const char* progname, const char* dev_path, int fd,
                      int ino_num, const char* inode_path, int inode_ftype);


int read_stamfs_inode_first_data_block(const char* progname,
                                       const char* dev_path, int fd,
                                       int ino_num, const char* inode_path,
                                       int data_block_num)
{
        char buf[STAMFS_BLOCK_SIZE];
        struct stamfs_dir_rec* stamfs_dr;
        int rc;
        int i;
        char block_name[1024];

        sprintf(block_name, "data block of inode %d", ino_num);

        /* we need to read from block #data_block_num. */
        rc = read_stamfs_block(progname, dev_path, fd,
                               block_name, data_block_num,
                               (char*)buf, sizeof(buf));
        if (!rc)
                return 0;

        printf("    Entries:\n");
        stamfs_dr = (struct stamfs_dir_rec*)buf;
        for (i=0 ;
             ((char*)stamfs_dr) < buf + STAMFS_BLOCK_SIZE;
             stamfs_dr++,i++) {
                char dir_name_str[STAMFS_MAX_FNAME_LEN+1];
                if (stamfs_dr->dr_ino == 0)
                        break;
                if (stamfs_dr->dr_ino == STAMFS_FREE_DIR_REC_MARKER)
                        continue;
                memcpy(dir_name_str,
                       stamfs_dr->dr_name,
                       stamfs_dr->dr_name_len);
                dir_name_str[stamfs_dr->dr_name_len] = '\0';

                printf("        Entry %d:\n", i);
                printf("            inode: %u\n", stamfs_dr->dr_ino);
                printf("            namelen: %d\n", stamfs_dr->dr_name_len);
                printf("            name: '%s'\n", dir_name_str);
                printf("            ftype: %d\n", stamfs_dr->dr_ftype);
        }

        /* recurse through the dir structure. */
        stamfs_dr = (struct stamfs_dir_rec*)buf;
        for (i=0 ;
             ((char*)stamfs_dr) < buf + STAMFS_BLOCK_SIZE;
             stamfs_dr++,i++) {
                char dir_name_str[STAMFS_MAX_FNAME_LEN+1];
                if (stamfs_dr->dr_ino == 0)
                        break;
                if (stamfs_dr->dr_ino == STAMFS_FREE_DIR_REC_MARKER)
                        break;
                memcpy(dir_name_str,
                       stamfs_dr->dr_name,
                       stamfs_dr->dr_name_len);
                dir_name_str[stamfs_dr->dr_name_len] = '\0';

                /* recurse */
                if (!read_stamfs_inode(progname, dev_path, fd,
                                       stamfs_dr->dr_ino, dir_name_str,
                                       stamfs_dr->dr_ftype))
                        return 0;
        }

        return 1;
}

int read_stamfs_inode_block_index(const char* progname, const char* dev_path,
                                  int fd, int ino_num, const char* inode_path,
                                  int inode_ftype, int index_block_num)
{
        struct stamfs_inode_block_index stamfs_bi;
        int rc;
        char block_name[1024];

        sprintf(block_name, "block index of inode %d", ino_num);

        /* we need to read from block #index_block_num. */
        rc = read_stamfs_block(progname, dev_path, fd,
                               "super-block", index_block_num,
                               (char*)&stamfs_bi, sizeof(stamfs_bi));
        if (!rc)
                return 0;

        printf("    1st_data_block_num: %d\n", stamfs_bi.index[0]);

        if (inode_ftype == STAMFS_DIR_REC_FTYPE_DIR)
                return read_stamfs_inode_first_data_block(progname, dev_path,
                                                          fd, ino_num,
                                                          inode_path,
                                                          stamfs_bi.index[0]);
        else
                return 1;
}

int read_stamfs_inode(const char* progname, const char* dev_path, int fd,
                      int ino_num, const char* inode_path, int inode_ftype)
{
        struct stamfs_inode stamfs_ino;
        int inode_block_num = stamfs_ii.index[ino_num-1];
        int rc;
        char block_name[1024];

        sprintf(block_name, "inode %d", ino_num);

        /* we need to read from block #inode_block_num. */
        rc = read_stamfs_block(progname, dev_path, fd,
                               block_name, inode_block_num,
                               (char*)&stamfs_ino, sizeof(stamfs_ino));
        if (!rc)
                return 0;

        printf("Inode '%s' (%d):\n", inode_path, ino_num);
        printf("    mode: %o\n", stamfs_ino.i_mode);
        printf("    uid: %d\n", stamfs_ino.i_uid);
        printf("    gid: %d\n", stamfs_ino.i_gid);
        printf("    size: %d\n", stamfs_ino.i_size);
        printf("    atime: %u\n", stamfs_ino.i_atime);
        printf("    mtime: %u\n", stamfs_ino.i_mtime);
        printf("    ctime: %u\n", stamfs_ino.i_ctime);
        printf("    num_blocks: %d\n", stamfs_ino.i_num_blocks);
        printf("    num_links: %d\n", stamfs_ino.i_num_links);
        printf("    index_block_num: %d\n", stamfs_ino.i_index_block);

        return read_stamfs_inode_block_index(progname, dev_path, fd,
                                             ino_num, inode_path, inode_ftype,
                                             stamfs_ino.i_index_block);
}

int stamfs2txt(const char* progname, const char* dev_path)
{
        int fd = open(dev_path, O_RDONLY | O_EXCL);

        if (fd == -1) {
                int errnum = errno;
                fprintf(stderr,
                        "%s: failed opening file '%s' for reading - %s'n",
                        progname,
                        dev_path,
                        strerror(errnum));
                return 0;
        }

        if (!read_stamfs_super_block(progname, dev_path, fd)) {
                close(fd);
                return 0;
        }

        if (!read_stamfs_inode_index(progname, dev_path, fd)) {
                close(fd);
                return 0;
        }

        if (!read_stamfs_free_list_block(progname, dev_path, fd)) {
                close(fd);
                return 0;
        }

        if (!read_stamfs_inode(progname, dev_path, fd,
                               STAMFS_ROOT_INODE_NUM, "/",
                               STAMFS_DIR_REC_FTYPE_DIR)) {
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
        if (!check_dev(progname, dev_path, force))
                exit(1);

        /* read the file system. */
        if (!stamfs2txt(progname, dev_path))
                exit(1);

        return 0;
}
