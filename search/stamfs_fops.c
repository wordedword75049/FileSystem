
#include <linux/fs.h>
#include <linux/dcache.h>

#include "stamfs.h"
#include "stamfs_super.h"
#include "stamfs_dir.h"
#include "stamfs_fops.h"
#include "stamfs_util.h"

/*
 * Data structures.
 */

struct file_operations stamfs_dir_fops = {
        readdir:        stamfs_readdir,
};

/*
 * This function is used for reading the contents of a directory, and
 * passing it back to the user.
 */
int stamfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
        struct dentry *dentry = filp->f_dentry;
        struct inode *dir = filp->f_dentry->d_inode;
        struct super_block* sb = dir->i_sb;
        int need_revalidation = (filp->f_version != dir->i_version);
        struct buffer_head *bh = NULL;
        int data_block_num = 0;
        struct stamfs_dir_rec *dir_rec;
        int err = 0;
        int over;

        STAMFS_DBG(DEB_STAM, "stamfs: readdir, file=%s, pos=%llu\n",
                             dentry->d_name.name, filp->f_pos);

        /* we have no problem with an empty dir (which should contain '.'
         * and '..', since we always allocate one block for the dir's data. */
        if (filp->f_pos > dir->i_size - sizeof(struct stamfs_dir_rec)) {
                STAMFS_DBG(DEB_STAM,
                           "stamfs: file pos larger then dir size.\n");
                goto done;
        }

        /* TODO - what does this revalidation, and version information,
         * mean at all? */
        if (need_revalidation) {
                /* TODO - make sure 'pos' points to the beginning of a dir rec. */
                need_revalidation = 0;
        }

        /* special handling for '.' and '..' */
        if (filp->f_pos == 0) {
                STAMFS_DBG(DEB_STAM,
                           "stamfs: readdir, f_pos == 0, adding '.', ino=%lu\n",
                           dir->i_ino);
                over = filldir(dirent, ".", 1, filp->f_pos,
                               dir->i_ino, DT_DIR);
                if (over < 0)
                        goto done;
                filp->f_pos++;
        }
        if (filp->f_pos == 1) {
                STAMFS_DBG(DEB_STAM, "stamfs: readdir, f_pos == 1, "
                                     "adding '..', ino=%lu\n",
                                     dentry->d_parent->d_inode->i_ino);
                over = filldir(dirent, "..", 2, filp->f_pos,
                               dentry->d_parent->d_inode->i_ino, DT_DIR);
                if (over < 0)
                        goto done;
                filp->f_pos++;
        }

        /* read in the data block of this directory. */
        err = stamfs_dir_get_data_block_num(dir, &data_block_num);
        STAMFS_DBG(DEB_STAM, "stamfs: block index in block %d\n",
                             data_block_num);
        if (!(bh = bread(sb->s_dev, data_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read block-index block.\n");
                err = -EIO;
                goto done;
        }

        /* loop over the dir entries, until we finish scanning, or until
         * we finish filling the dirent's available space */
        /* note: the '- 2' is because of the phony "." and ".." entries. */
        dir_rec = (struct stamfs_dir_rec *)(((char*)(bh->b_data)) + filp->f_pos - 2);
        while (1) {
                unsigned char d_type = DT_UNKNOWN;
                /* dr_ino == 0 implies end-of-list. */
                if (dir_rec->dr_ino == 0)
                        break;

                /* add this entry, unless it's marked as 'free'. */
                if (dir_rec->dr_ino != STAMFS_FREE_DIR_REC_MARKER) {
                        if (dir_rec->dr_ftype == STAMFS_DIR_REC_FTYPE_DIR)
                                d_type = DT_DIR;
                        if (dir_rec->dr_ftype == STAMFS_DIR_REC_FTYPE_FILE)
                                d_type = DT_REG;

                        over = filldir(dirent, dir_rec->dr_name,
                                       dir_rec->dr_name_len, filp->f_pos,
                                       le32_to_cpu(dir_rec->dr_ino), d_type);
                        if(over < 0)
                                goto done;

                        STAMFS_DBG(DEB_STAM, "stamfs: readdir, f_pos == %lld, "
                                        "adding '%s', ino=%u\n",
                                        filp->f_pos, dir_rec->dr_name,
                                        le32_to_cpu(dir_rec->dr_ino));
                }

                /* skip to the next entry. */
                filp->f_pos += sizeof(struct stamfs_dir_rec);

                /* note: again, the '+ 2' is because of the
                 * phony "." and ".." entries. */
                if (filp->f_pos >= STAMFS_BLOCK_SIZE + 2)
                        goto done;
                dir_rec++;
        }

  done:
        filp->f_version = dir->i_version;
        UPDATE_ATIME(dir);

        if (bh)
                brelse(bh);

        return err;
}
