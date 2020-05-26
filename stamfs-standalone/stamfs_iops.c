/* This file is part of stamfs, a GNU/Linux kernel module that */
/* implements a file-system for accessing STAM devices.        */
/*                                                             */
/* Copyright (C) 2004 guy keren, choo@actcom.co.il             */
/* License: GNU General Public License                         */

/*
 *$Id$
 */

#include <linux/slab.h>

#include "stamfs_iops.h"

#include "stamfs.h"
#include "stamfs_super.h"
#include "stamfs_inode.h"
#include "stamfs_dir.h"
#include "stamfs_iops.h"
#include "stamfs_fops.h"
#include "stamfs_aops.h"
#include "stamfs_util.h"

/*
 * Data structures.
 */

struct inode_operations stamfs_dir_iops = {
        create:         stamfs_iop_create,
        lookup:         stamfs_iop_lookup,
        /*link: we don't support hardlink. */
        unlink:         stamfs_iop_unlink,
        /*symlink: we don't support symbolic links, either. */
        mkdir:          stamfs_iop_mkdir,
        rmdir:          stamfs_iop_rmdir,
        /*mknod: we don't support special file types, too. */
        rename:         stamfs_iop_rename
};

struct inode_operations stamfs_file_iops = {
        truncate:       stamfs_inode_truncate
};


/*
 * Utility functions.
 */

/*
 * initialize the block index of the given inode.
 * returns a negative error code, in case of an error, 0 on success.
 */
int stamfs_inode_init_block_index(struct inode *inode, int bi_block_num)
{
        int err = 0;
        struct super_block *sb = inode->i_sb;
        struct buffer_head *bibh = NULL;
        struct stamfs_inode_block_index *stamfs_bi = NULL;

        /* read the inode's block index. */
        if (!(bibh = bread(sb->s_dev, bi_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read inode block index, block %d.\n",
                       bi_block_num);
                err = -EIO;
                goto ret;
        }
        stamfs_bi = (struct stamfs_inode_block_index *)((char *)(bibh->b_data));
        memset(stamfs_bi->index, 0, sizeof(*stamfs_bi));
        mark_buffer_dirty_inode(bibh, inode);

  ret:
        if (bibh)
                brelse(bibh);
        return err;
}

/*
 * Allocate a new inode, to be used when creating a new file or directory.
 */
struct inode *stamfs_inode_new_inode(struct super_block *sb, int mode)
{
        struct inode *child_ino = NULL;
        ino_t ino_num = 0;
        int inode_block_num = 0;
        int bi_block_num = 0;
        int err = 0;
        struct stamfs_inode_meta_data *stamfs_inode_meta = NULL;

        /* allocate a disk block to contain this inode's data. */
        inode_block_num = stamfs_alloc_block(sb);
        if (inode_block_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

        /* allocate a disk block to contain the block index of this inode. */
        bi_block_num = stamfs_alloc_block(sb);
        if (bi_block_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

        /* allocate a free inode number. */
        ino_num = stamfs_alloc_inode_num(sb, inode_block_num);
        if (ino_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

        /* allocate a VFS inode object. */
        child_ino = new_inode(sb);
        if (!child_ino) {
                STAMFS_DBG(DEB_STAM,
                           "stamfs: new_inode() failed... inode %lu\n",
                           ino_num);
                err = -ENOMEM;
                goto ret_err;
        }

        /* initialize the inode's block index. */
        err = stamfs_inode_init_block_index(child_ino, bi_block_num);
        if (err)
                goto ret_err;

        /* init the inode's data. */
        child_ino->i_ino = ino_num;
        child_ino->i_mode = mode;
        child_ino->i_nlink = 1;  /* this inode will be stored in a directory,
                                  * so there's at least one link to this inode,
                                  * from that directory. */
        child_ino->i_size = 0;
        child_ino->i_blksize = STAMFS_BLOCK_SIZE;
        child_ino->i_blkbits = 10;
        child_ino->i_blocks = 0;
        child_ino->i_uid = current->fsuid;
        child_ino->i_gid = current->fsgid;
        child_ino->i_atime = child_ino->i_mtime = child_ino->i_ctime = CURRENT_TIME;
        child_ino->i_attr_flags = 0;

        /* init the inode's STAMFS meta data. */
        stamfs_inode_meta = kmalloc(sizeof(struct stamfs_inode_meta_data),
                                    GFP_KERNEL);
        if (!stamfs_inode_meta) {
                printk("stamfs: not enough memory to allocate inode meta data.\n");
                goto ret_err;
        }
        stamfs_inode_meta->i_block_num = inode_block_num;
        stamfs_inode_meta->i_bi_block_num = bi_block_num;

        child_ino->u.generic_ip = stamfs_inode_meta;

        /* set the inode operations structs. */
        if (S_ISREG(child_ino->i_mode)) {
                child_ino->i_op = &stamfs_file_iops;
                child_ino->i_fop = &stamfs_file_fops;
                child_ino->i_mapping->a_ops = &stamfs_aops;
        }
        else if (S_ISDIR(child_ino->i_mode)) {
                child_ino->i_op = &stamfs_dir_iops;
                child_ino->i_fop = &stamfs_dir_fops;
                child_ino->i_mapping->a_ops = &stamfs_aops;
        }

        insert_inode_hash(child_ino);
        /* make sure the inode gets written to disk by the inodes cache. */
        mark_inode_dirty(child_ino);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        if (child_ino)
                iput(child_ino); /* child_ino will be deleted here. */
        if (ino_num > 0)
                stamfs_release_inode_num(sb, ino_num);
        if (inode_block_num > 0)
                stamfs_release_block(sb, inode_block_num);
        if (bi_block_num > 0)
                stamfs_release_block(sb, bi_block_num);
  ret:
        return (err == 0 ? child_ino : ERR_PTR(err));
}

/*
 * Given an inode and a block offset, sets p_block_number to the block number
 * containing this block offset, or -1 if there is no block mapped at the
 * given offset.
 * returns 0 on success or a negative error code on failure.
 */
int stamfs_inode_block_offset_to_number(struct inode *ino, int block_offset,
                                        int *p_block_num)
{
        int err = 0;
        struct super_block *sb = ino->i_sb;
        struct stamfs_inode_meta_data *inode_meta = STAMFS_INODE_META(ino);
        int bi_block_num = inode_meta->i_bi_block_num;
        struct buffer_head *bibh = NULL;
        struct stamfs_inode_block_index *stamfs_bi = NULL;
        unsigned int block_num;

        STAMFS_DBG(DEB_STAM, "stamfs: for inode %lu, "
                             "getting block number for block offset %d\n",
                             ino->i_ino, block_offset);

        /* read the inode's block index. */
        if (!(bibh = bread(sb->s_dev, bi_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read inode block index, block %d.\n",
                       bi_block_num);
                err = -EIO;
                goto ret;
        }
        stamfs_bi = (struct stamfs_inode_block_index *)((char *)(bibh->b_data));

        block_num = le32_to_cpu(stamfs_bi->index[block_offset]);
        if (block_num != STAMFS_FREE_BLOCK_MARKER && block_num != 0) {
                STAMFS_DBG(DEB_STAM, "stamfs: block number %u\n", block_num);
                *p_block_num = block_num;
        }
        else {
                STAMFS_DBG(DEB_STAM, "stamfs: block not mapped\n");
                *p_block_num = -1;
        }

        /* all went well... */
        err = 0;
        goto ret;

  ret:
        if (bibh)
                brelse(bibh);
        return err;
}

/*
 * given an inode, maps the given block offset to the given block number.
 * returns 0 on success or a negative error code on failure.
 */
int stamfs_inode_map_block_offset_to_number(struct inode *ino,
                                            int block_offset, int block_num)
{
        int err = 0;
        struct super_block *sb = ino->i_sb;
        struct stamfs_inode_meta_data *inode_meta = STAMFS_INODE_META(ino);
        int bi_block_num = inode_meta->i_bi_block_num;
        struct buffer_head *bibh = NULL;
        struct stamfs_inode_block_index *stamfs_bi = NULL;

        STAMFS_DBG(DEB_STAM, "stamfs: for inode %lu, "
                             "mapping block offset %d to block number %d\n",
                             ino->i_ino, block_offset, block_num);

        /* read the inode's block index. */
        if (!(bibh = bread(sb->s_dev, bi_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read inode block index, block %d.\n",
                       bi_block_num);
                err = -EIO;
                goto ret;
        }
        stamfs_bi = (struct stamfs_inode_block_index *)((char *)(bibh->b_data));

        /* store the new mapping. */
        stamfs_bi->index[block_offset] = cpu_to_le32(block_num);
        mark_buffer_dirty_inode(bibh, ino);

        /* the inode was changed as well - but not the size (which is a logical
         * value, thus not related to which of the blocks are actually
         * mapped). */
        ino->i_blocks += 1;
        mark_inode_dirty(ino);

        /* all went well... */
        err = 0;
        goto ret;

  ret:
        if (bibh)
                brelse(bibh);
        return err;
}

/*
 * Add the given file to the given directory, and instantiate the child in
 * the dcache.
 */
static int stamfs_add_file(struct inode *dir, struct inode *child,
                           struct dentry *child_dentry)
{
        int err = stamfs_dir_add_link(dir, child,
                                      child_dentry->d_name.name,
                                      child_dentry->d_name.len);
        if (!err) {
                d_instantiate(child_dentry, child);
                return 0;
        }

        /* on error: */
        child->i_nlink--;
        mark_inode_dirty(child);
        iput(child);

        return err;
}


/*
 * The inode operations functions.
 */

/*
 * Create the file whose name is found in the given dentry, inside the given
 * directory, with the given access permissions.
 */
int stamfs_iop_create(struct inode *dir, struct dentry *dentry, int mode)
{
        struct inode *ino = NULL;
        int err = 0;

        STAMFS_DBG(DEB_STAM, "stamfs: create inode %ld, path=%s, mode=%o\n",
                             dir->i_ino, dentry->d_name.name, mode);

        /* allocate an inode for the child, and add it to the directory. */
        ino = stamfs_inode_new_inode (dir->i_sb, mode);
        err = PTR_ERR(ino);
        if (!IS_ERR(ino)) {
                err = stamfs_add_file(dir, ino, dentry);
        }

        return err;
}

/*
 * Look for an inode whose name is in the given dentry, inside the given
 * directory.
 * @return an instantiate dentry for the child on success, a negative error
 *         code on failure.
 */
struct dentry *stamfs_iop_lookup(struct inode *dir, struct dentry *dentry)
{
        int err = 0;
        struct inode* ino = NULL;
        ino_t ino_num = 0;

        STAMFS_DBG(DEB_STAM, "stamfs: lookup inode, path=%s\n",
                             dentry->d_name.name);

        /* sanity checks. */
        if (dentry->d_name.len > STAMFS_MAX_FNAME_LEN) {
                STAMFS_DBG(DEB_STAM, "stamfs: name too long.\n");
                return ERR_PTR(-ENAMETOOLONG);
        }

        /* find the inode of the requested file. */
        err = stamfs_dir_get_file_by_name(dir, dentry->d_name.name,
                                          dentry->d_name.len, &ino_num);
        if (err == 0 && ino_num > 0) {
                ino = iget(dir->i_sb, ino_num);
                if (!ino) {
                        STAMFS_DBG(DEB_STAM, "stamfs: iget failed.\n");
                        return ERR_PTR(-EACCES);
                }
        }
        else {
                STAMFS_DBG(DEB_STAM, "stamfs: file not found.\n");
        }

        d_add(dentry, ino);

        STAMFS_DBG(DEB_STAM, "stamfs: after d_add, dentry->d_count==%d\n",
                             atomic_read(&dentry->d_count));

        return (err == 0 ? NULL : ERR_PTR(err));
}

/*
 * Unlink the inode whose name is in the given dentry, from the given directory.
 */
int stamfs_iop_unlink(struct inode *dir, struct dentry *dentry)
{
        int err = 0;
        struct inode *child = dentry->d_inode;
        const char* child_name = dentry->d_name.name;
        int child_name_len = dentry->d_name.len;

        STAMFS_DBG(DEB_STAM, "stamfs: unlink inode %ld, path=%s\n",
                             dir->i_ino, dentry->d_name.name);

        err = stamfs_dir_del_link(dir, child, child_name, child_name_len);
        if (err != 0)
                goto ret_err;

        /* the child ws updated, and has now one less link pointing to it. */
        /* if its link count will drop to zero, the VFS will delete it.    */
        child->i_ctime = dir->i_ctime;
        child->i_nlink--;
        mark_inode_dirty(child);
        
        STAMFS_DBG(DEB_STAM, "stamfs: parent_i_nlink=%d, child_i_nlink=%d\n",
                             dir->i_nlink, child->i_nlink);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        /* fall through... */
  ret:
        return err;
}

/*
 * Create a directory whose name is in the given dentry and with the given
 * access mode, in the given directory.
 */
int stamfs_iop_mkdir(struct inode *parent_dir, struct dentry *dentry, int mode)
{
        int err;
        struct inode *child_dir = NULL;
        int parent_dir_inc = 0;
        int child_dir_inc = 0;

        STAMFS_DBG(DEB_STAM, "stamfs: mkdir inode, path=%s, mode=%o\n",
                             dentry->d_name.name, mode);

        /* increase the link count of the parent dir (the child dir will have
         * a '..' entry pointing back to the parent dir). */
        parent_dir->i_nlink++;
        mark_inode_dirty(parent_dir);
        parent_dir_inc++;

        child_dir = stamfs_inode_new_inode(parent_dir->i_sb, S_IFDIR | mode);
        err = PTR_ERR(child_dir);
        if (IS_ERR(child_dir))
                goto ret;

        /* increase the link-count of the child dir - it has a "." entry
         * pointing to itself. */
        child_dir->i_nlink++;
        mark_inode_dirty(child_dir);

        /* create the child dir on disk, as an empty directory. */
        err = stamfs_inode_make_empty_dir(child_dir);
        if (err)
                goto ret_err;

        /* link the child as an inode in the parent. */
        err = stamfs_dir_add_link(parent_dir, child_dir,
                                  dentry->d_name.name, dentry->d_name.len);
        if (err)
                goto ret_err;

        /* finally, instantiate the child's dentry. */
        d_instantiate(dentry, child_dir);

        STAMFS_DBG(DEB_STAM,
                   "stamfs: after d_instantiate of child, dentry->d_count==%d\n",
                   atomic_read(&dentry->d_count));

        STAMFS_DBG(DEB_STAM, "stamfs: parent_i_nlink=%d, child_i_nlink=%d\n",
                             parent_dir->i_nlink, child_dir->i_nlink);

        /* all is well... */
        err = 0;
        goto ret;

  ret_err:
        if (parent_dir_inc > 0) {
                parent_dir->i_nlink--;
                mark_inode_dirty(parent_dir);
        }
        if (child_dir_inc > 0) {
                child_dir->i_nlink -= 2;
                mark_inode_dirty(child_dir);
        }
        if (child_dir)
                iput(child_dir); /* child_dir will be deleted here. */
  ret:
        return err;
}

/*
 * Remove the directory whose name is in the given dentry, from the given
 * directory.
 */
int stamfs_iop_rmdir(struct inode *parent_dir, struct dentry *dentry)
{
        int err = 0;
        struct inode *child_dir = dentry->d_inode;

        STAMFS_DBG(DEB_STAM, "stamfs: rmdir inode %ld, path=%s\n",
                             parent_dir->i_ino, dentry->d_name.name);

        /* a non-emptry directory may not be deleted. */
        err = stamfs_dir_is_empty(child_dir);
        if (err < 0) {
                STAMFS_DBG(DEB_STAM, "stamfs: encountered a problem "
                                     "when checking if child dir is empty.\n");
                goto ret_err;
        }
        if (err != 0) {
                STAMFS_DBG(DEB_STAM, "stamfs: child dir is not empty.\n");
                err = -ENOTEMPTY;
                goto ret_err;
        }

        /* child dir is empty - carry on its deletion. */
        err = stamfs_iop_unlink(parent_dir, dentry);
        if (err)
                goto ret_err;

        child_dir->i_size = 0;

        /* the parent no longer points to the cihld, and the child's '..'
         * entry no longer points to the parent. */
        parent_dir->i_nlink--;
        mark_inode_dirty(parent_dir);
        child_dir->i_nlink--;
        mark_inode_dirty(child_dir);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        /* fall through... */
  ret:
        return err;
}

/*
 * Rename the file in directory old_dir whose name is given in 'old_dentry',
 * and place it in direcotry 'new_dir' (possibly the same as 'old_dir'), with
 * the new name found in 'new_dentry' (could be the same file name - but never
 * the same dentry).
 */
int stamfs_iop_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
        /* TODO */
        STAMFS_DBG(DEB_STAM,
                   "stamfs: rename inode %ld, old_path=%s, new_path=%s\n",
                   old_dir->i_ino,
                   old_dentry->d_name.name,
                   new_dentry->d_name.name);

        return -EPERM;
}
