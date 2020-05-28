

#include <linux/sched.h>

#include "stamfs_dir.h"

#include "stamfs.h"
#include "stamfs_super.h"
#include "stamfs_inode.h"
#include "stamfs_util.h"

/*
 * return the block number of the (first) data block of the given directory.
 * returns a negative error code, in case of an error, 0 on success.
 */
int stamfs_dir_get_data_block_num(struct inode *dir, int *p_data_block_num)
{
        int err = 0;
        struct super_block *sb = dir->i_sb;
        struct stamfs_inode_meta_data *inode_meta = STAMFS_INODE_META(dir);
        int bi_block_num = inode_meta->i_bi_block_num;
        struct buffer_head *bibh = NULL;
        struct stamfs_inode_block_index *stamfs_bi = NULL;

        /* read the directory's block index. */
        if (!(bibh = bread(sb->s_dev, bi_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read inode block index, block %d.\n",
                       bi_block_num);
                err = -EIO;
                goto ret;
        }
        stamfs_bi = (struct stamfs_inode_block_index *)((char *)(bibh->b_data));
        (*p_data_block_num) = le32_to_cpu(stamfs_bi->index[0]);

  ret:
        if (bibh)
                brelse(bibh);
        return err;
}

/*
 * Given a directory's inode and a file-name, returns the inode number of
 * this file.
 * @return 0 on success, a negative error value in case of an error, or if
 *         there is no file with that name in this directory.
 */
int stamfs_dir_get_file_by_name(struct inode *dir, const char *name,
                                int namelen, ino_t* p_ino_num)
{
        int err = 0;
        struct buffer_head *bh = NULL;
        struct super_block* sb = dir->i_sb;
        struct stamfs_dir_rec *dir_rec = NULL;
        int data_block_num = 0;

        STAMFS_DBG(DEB_STAM,
                   "stamfs: getting file '%s', namelen=%d, dir_inode=%lu\n",
                   name, namelen, dir->i_ino);

        err = stamfs_dir_get_data_block_num(dir, &data_block_num);
        if (err)
                goto ret_err;

        /* read in the data block of this directory. */
        STAMFS_DBG(DEB_STAM, "stamfs: dir data in block %d\n", data_block_num);
        if (!(bh = bread(sb->s_dev, data_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read dir data block.\n");
                err = -EIO;
                goto ret_err;
        }

        /* scan the data block, looking for the given file. */
        *p_ino_num = 0;
        dir_rec = (struct stamfs_dir_rec *)((char*)(bh->b_data));
        for ( ; ((char*)dir_rec) < ((char*)bh->b_data) + STAMFS_BLOCK_SIZE; dir_rec++) {
                STAMFS_DBG(DEB_STAM,
                           "stamfs: next entry: ino=%d, name=%s, namelen=%d\n",
                           dir_rec->dr_ino,
                           (dir_rec->dr_ino ? dir_rec->dr_name : "(null)"),
                           dir_rec->dr_name_len);
                if (dir_rec->dr_ino == 0)
                        break; /* last entry. */
                if (le32_to_cpu(dir_rec->dr_ino) == STAMFS_FREE_DIR_REC_MARKER)
                        continue; /* empty entry. */
                if (dir_rec->dr_name_len != namelen)
                        continue;
                if (memcmp(dir_rec->dr_name, name, namelen) != 0)
                        continue;
                /* we have a match. */
                *p_ino_num = dir_rec->dr_ino;
                break;
        }

        /* all went well... */
        goto ret;

  ret_err:
        /* fall through. */
  ret:
        if (bh)
                brelse(bh);
        STAMFS_DBG(DEB_STAM, "stamfs: returning %d, *p_ino_num=%lu\n",
                             err, *p_ino_num);
        return err;
}
