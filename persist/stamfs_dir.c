

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
 * set the block number of the (first) data block of the given directory.
 * returns a negative error code, in case of an error.
 */
static int stamfs_dir_set_data_block_num(struct inode *dir, int data_block_num)
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
        stamfs_bi->index[0] = cpu_to_le32(data_block_num);
        mark_buffer_dirty_inode(bibh, dir);

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

/*
 * Given a directory's inode, create the data part of this directory on disk,
 * as an empty directory.
 * Note: we assume that the inode itself was already created.
 */
int stamfs_inode_make_empty_dir(struct inode *dir)
{
        int err = 0;
        struct super_block *sb = dir->i_sb;
        struct buffer_head *data_bh = NULL;
        struct stamfs_dir_rec *last_dir_rec = NULL;
        int data_block_num = 0;

        /* allocate a data block. */
        data_block_num = stamfs_alloc_block(sb);
        if (data_block_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

        /* initialize the directory's dir records block. */
        if (!(data_bh = bread(sb->s_dev, data_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read block %d.\n", data_block_num);
                err = -EIO;
                goto ret_err;
        }

        /* the first entry should have its 'dr_ino' field set to 0, to mark
         * end-of-list. */
        last_dir_rec = (struct stamfs_dir_rec *)((char*)(data_bh->b_data));
        memset(last_dir_rec, 0, sizeof(struct stamfs_dir_rec));
        last_dir_rec->dr_ino = 0;
        mark_buffer_dirty(data_bh);
        buffer_insert_inode_data_queue(data_bh, dir);

        /* mark this block as the first (and only) data block of the directory. */
        err = stamfs_dir_set_data_block_num(dir, data_block_num);
        if (err)
                goto ret_err;

        dir->i_size += STAMFS_BLOCK_SIZE;
        dir->i_blocks++;
        dir->i_mtime = dir->i_ctime = CURRENT_TIME;
        mark_inode_dirty(dir);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        if (data_block_num > 0)
                stamfs_release_block(sb, data_block_num);
  ret:
        if (data_bh)
                brelse(data_bh);
        return err;
}

/*
 * Given a directory's inode and a child inode, add this child inode as an
 * entry in the directory's data.
 * @return 0 on success, a negative error code on failure.
 */
int stamfs_dir_add_link(struct inode *parent_dir, struct inode *child,
                        const char *name, int namelen)
{
        int err = 0;
        struct super_block *sb = parent_dir->i_sb;
        struct buffer_head *data_bh = NULL;
        struct stamfs_dir_rec *last_dir_rec = NULL;
        int data_block_num = 0;

        /* TODO - handle directories with more then one data block... */

        STAMFS_DBG(DEB_STAM,
                   "stamfs: adding link, inode %lu -> inode %lu, name=%s\n",
                   parent_dir->i_ino, child->i_ino, name);

        /* sanity checks. */
        if (namelen > STAMFS_MAX_FNAME_LEN) {
                err = -ENAMETOOLONG;
                goto ret_err;
        }

        err = stamfs_dir_get_data_block_num(parent_dir, &data_block_num);
        if (err)
                goto ret_err;

        /* read in the data block of the parent directory. */
        STAMFS_DBG(DEB_STAM, "stamfs: dir data in block %d\n", data_block_num);
        if (!(data_bh = bread(sb->s_dev, data_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read dir data block.\n");
                err = -EIO;
                goto ret;
        }

        /* find the last entry in the parent directory. */
        /* TODO - if we find a freed entry in the middle of the list - we
         * should use it instead. */
        last_dir_rec = (struct stamfs_dir_rec *)((char*)(data_bh->b_data));
        for ( ; ((char*)last_dir_rec) < ((char*)data_bh->b_data) + STAMFS_BLOCK_SIZE; last_dir_rec++) {
                if (last_dir_rec->dr_ino == 0)
                        break; /* last entry found. */
        }

        /* if no free entry found... */
        if (((char*)last_dir_rec) >= ((char*)data_bh->b_data) + STAMFS_BLOCK_SIZE) {
                err = -ENOSPC;
                goto ret_err;
        }

        /* ok, populate the entry. */
        last_dir_rec->dr_ino = cpu_to_le32(child->i_ino);
        last_dir_rec->dr_name_len = namelen;
        if (S_ISDIR(child->i_mode))
                last_dir_rec->dr_ftype = STAMFS_DIR_REC_FTYPE_DIR;
        else
                last_dir_rec->dr_ftype = STAMFS_DIR_REC_FTYPE_FILE;
        memcpy(last_dir_rec->dr_name, name, namelen);

        /* mark the next entry as the last one, in case it contains 
         * stale data. */
        last_dir_rec++;
        if (((char*)last_dir_rec) < ((char*)data_bh->b_data) + STAMFS_BLOCK_SIZE)
                last_dir_rec->dr_ino = cpu_to_le32(0);

        mark_buffer_dirty(data_bh);
        buffer_insert_inode_data_queue(data_bh, parent_dir);

        parent_dir->i_mtime = parent_dir->i_ctime = CURRENT_TIME;
        mark_inode_dirty(parent_dir);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        /* fallthrough */
  ret:
        if (data_bh)
                brelse(data_bh);
        return err;
}

/*
 * Given a directory's inode and a child inode, remove this child inode from
 * the directory's list-of-entries.
 * @return 0 on success, a negative error code on failure.
 */
int stamfs_dir_del_link(struct inode *parent_dir, struct inode *child,
                        const char *name, int namelen)
{
        int err = 0;
        struct super_block *sb = parent_dir->i_sb;
        struct buffer_head *data_bh = NULL;
        struct stamfs_dir_rec *dir_rec = NULL;
        struct stamfs_dir_rec *next_dir_rec = NULL;
        int data_block_num = 0;
        int found_child = 0;

        /* TODO - handle directories with more then one data block... */

        STAMFS_DBG(DEB_STAM,
                   "stamfs: removing link, inode %lu -/-> inode %lu, name=%s\n",
                   parent_dir->i_ino, child->i_ino, name);

        err = stamfs_dir_get_data_block_num(parent_dir, &data_block_num);
        if (err)
                goto ret_err;

        /* read in the data block of the parent directory. */
        STAMFS_DBG(DEB_STAM, "stamfs: dir data in block %d\n", data_block_num);
        if (!(data_bh = bread(sb->s_dev, data_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read dir data block.\n");
                err = -EIO;
                goto ret_err;
        }

        /* find the child's entry in the parent directory. */
        dir_rec = (struct stamfs_dir_rec *)((char*)(data_bh->b_data));
        for ( ; ((char*)dir_rec) < ((char*)data_bh->b_data) + STAMFS_BLOCK_SIZE; dir_rec++) {
                STAMFS_DBG(DEB_STAM,
                           "stamfs: next entry: ino=%d, name=%s, namelen=%d\n",
                           dir_rec->dr_ino,
                           (dir_rec->dr_ino ? dir_rec->dr_name : "(null)"),
                           dir_rec->dr_name_len);
                if (le32_to_cpu(dir_rec->dr_ino) == 0)
                        break; /* last entry. */
                if (le32_to_cpu(dir_rec->dr_ino) == STAMFS_FREE_DIR_REC_MARKER)
                        continue; /* empty entry. */
                if (dir_rec->dr_name_len != namelen)
                        continue;
                if (memcmp(dir_rec->dr_name, name, namelen) != 0)
                        continue;
                /* we have a match. */
                found_child = 1;
                break;
        }

        if (!found_child) {
                err = -ENOENT;
                goto ret_err;
        }

        /* mark this entry as free, unless it's the one before last. */
        next_dir_rec = dir_rec + 1;
        if ( ((char*)next_dir_rec) < ((char*)data_bh->b_data) + STAMFS_BLOCK_SIZE && le32_to_cpu(next_dir_rec->dr_ino) != 0)
                dir_rec->dr_ino = cpu_to_le32(STAMFS_FREE_DIR_REC_MARKER);
        else
                dir_rec->dr_ino = cpu_to_le32(0);

        /* clear up the fields, just for safety. */
        dir_rec->dr_name_len = 0;
        dir_rec->dr_ftype = STAMFS_DIR_REC_FTYPE_UNKNOWN;
        dir_rec->dr_name[0] = '\0';
        mark_buffer_dirty(data_bh);
        buffer_insert_inode_data_queue(data_bh, parent_dir);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        /* fallthrough */
  ret:
        if (data_bh)
                brelse(data_bh);
        return err;
}

/*
 * Given a directory's inode, check if this directory is empty.
 * returns 0 if it is, 1 if it's not, a negative value in case of an error
 * (e.g. I/O error).
 */
int stamfs_dir_is_empty(struct inode *dir)
{
        int err = 0; /* assume the directory is empty. */
        struct super_block *sb = dir->i_sb;
        struct buffer_head *data_bh = NULL;
        struct stamfs_dir_rec *last_dir_rec = NULL;
        int data_block_num = 0;

        /* TODO - handle directories with more then one data block... */

        STAMFS_DBG(DEB_STAM, "stamfs: checking if dir empty,inode %lu\n",
                             dir->i_ino);

        err = stamfs_dir_get_data_block_num(dir, &data_block_num);
        if (err)
                goto ret_err;

        /* read in the data block of the directory. */
        STAMFS_DBG(DEB_STAM, "stamfs: dir data in block %d\n", data_block_num);
        if (!(data_bh = bread(sb->s_dev, data_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read dir data block.\n");
                err = -EIO;
                goto ret_err;
        }

        /* find if there's any entry in the directory. */
        last_dir_rec = (struct stamfs_dir_rec *)((char*)(data_bh->b_data));
        for ( ; ((char*)last_dir_rec) < ((char*)data_bh->b_data) + STAMFS_BLOCK_SIZE; last_dir_rec++) {
                if (le32_to_cpu(last_dir_rec->dr_ino) == STAMFS_FREE_DIR_REC_MARKER)
                        continue;
                if (le32_to_cpu(last_dir_rec->dr_ino) == 0)
                        break;
                /* found a real entry - not empty. */
                err = 1;
        }

        /* all went well... */
        goto ret;

  ret_err:
        /* fallthrough */
  ret:
        if (data_bh)
                brelse(data_bh);
        return err;
}
