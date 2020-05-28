

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
        lookup:         stamfs_iop_lookup,
        mkdir:          stamfs_iop_mkdir,
};


/*
 * Utility functions.
 */

/*
 * initialize the block index of the given inode.
 * returns a negative error code, in case of an error, 0 on success.
 */
int stamfs_inode_init_block_index(struct inode *ino, int bi_block_num)
{
        int err = 0;
        struct super_block *sb = ino->i_sb;
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
        mark_buffer_dirty_inode(bibh, ino);

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
                /* we're not handling files yet. */
                child_ino->i_op = (struct inode_operations *)NULL;
                child_ino->i_fop = (struct file_operations *)NULL;
                child_ino->i_mapping->a_ops = (struct address_space_operations *) NULL;
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
 * The inode operations functions.
 */

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
