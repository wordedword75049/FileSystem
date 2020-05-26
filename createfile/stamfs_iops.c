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

struct inode_operations stamfs_dir_iops = {
        create:         stamfs_iop_create,
        lookup:         stamfs_iop_lookup,
        unlink:         stamfs_iop_unlink,
        mkdir:          stamfs_iop_mkdir,
        rmdir:          stamfs_iop_rmdir,
};


struct inode_operations stamfs_file_iops = {
};


int stamfs_inode_init_block_index(struct inode *ino, int bi_block_num)
{
        int err = 0;
        struct super_block *sb = ino->i_sb;
        struct buffer_head *bibh = NULL;
        struct stamfs_inode_block_index *stamfs_bi = NULL;

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

struct inode *stamfs_inode_new_inode(struct super_block *sb, int mode)
{
        struct inode *child_ino = NULL;
        ino_t ino_num = 0;
        int inode_block_num = 0;
        int bi_block_num = 0;
        int err = 0;
        struct stamfs_inode_meta_data *stamfs_inode_meta = NULL;

        inode_block_num = stamfs_alloc_block(sb);
        if (inode_block_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

        bi_block_num = stamfs_alloc_block(sb);
        if (bi_block_num == 0) {
                err = -ENOSPC;
                goto ret_err;
        }

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

        err = stamfs_inode_init_block_index(child_ino, bi_block_num);
        if (err)
                goto ret_err;

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


        stamfs_inode_meta = kmalloc(sizeof(struct stamfs_inode_meta_data),
                                    GFP_KERNEL);
        if (!stamfs_inode_meta) {
                printk("stamfs: not enough memory to allocate inode meta data.\n");
                goto ret_err;
        }
        stamfs_inode_meta->i_block_num = inode_block_num;
        stamfs_inode_meta->i_bi_block_num = bi_block_num;

        child_ino->u.generic_ip = stamfs_inode_meta;

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

        mark_inode_dirty(child_ino);

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


int stamfs_iop_create(struct inode *dir, struct dentry *dentry, int mode)
{
        struct inode *ino = NULL;
        int err = 0;

        STAMFS_DBG(DEB_STAM, "stamfs: create inode %ld, path=%s, mode=%o\n",
                             dir->i_ino, dentry->d_name.name, mode);

        ino = stamfs_inode_new_inode (dir->i_sb, mode);
        err = PTR_ERR(ino);
        if (!IS_ERR(ino)) {
                err = stamfs_add_file(dir, ino, dentry);
        }

        return err;
}

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

        child->i_ctime = dir->i_ctime;
        child->i_nlink--;
        mark_inode_dirty(child);

        STAMFS_DBG(DEB_STAM, "stamfs: parent_i_nlink=%d, child_i_nlink=%d\n",
                             dir->i_nlink, child->i_nlink);

        err = 0;
        goto ret;

  ret_err:
  ret:
        return err;
}

int stamfs_iop_mkdir(struct inode *parent_dir, struct dentry *dentry, int mode)
{
        int err;
        struct inode *child_dir = NULL;
        int parent_dir_inc = 0;
        int child_dir_inc = 0;

        STAMFS_DBG(DEB_STAM, "stamfs: mkdir inode, path=%s, mode=%o\n",
                             dentry->d_name.name, mode);

        parent_dir->i_nlink++;
        mark_inode_dirty(parent_dir);
        parent_dir_inc++;

        child_dir = stamfs_inode_new_inode(parent_dir->i_sb, S_IFDIR | mode);
        err = PTR_ERR(child_dir);
        if (IS_ERR(child_dir))
                goto ret;

        child_dir->i_nlink++;
        mark_inode_dirty(child_dir);

        err = stamfs_inode_make_empty_dir(child_dir);
        if (err)
                goto ret_err;

        err = stamfs_dir_add_link(parent_dir, child_dir,
                                  dentry->d_name.name, dentry->d_name.len);
        if (err)
                goto ret_err;

        d_instantiate(dentry, child_dir);

        STAMFS_DBG(DEB_STAM,
                   "stamfs: after d_instantiate of child, dentry->d_count==%d\n",
                   atomic_read(&dentry->d_count));

        STAMFS_DBG(DEB_STAM, "stamfs: parent_i_nlink=%d, child_i_nlink=%d\n",
                             parent_dir->i_nlink, child_dir->i_nlink);

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
                iput(child_dir);
  ret:
        return err;
}

int stamfs_iop_rmdir(struct inode *parent_dir, struct dentry *dentry)
{
        int err = 0;
        struct inode *child_dir = dentry->d_inode;

        STAMFS_DBG(DEB_STAM, "stamfs: rmdir inode %ld, path=%s\n",
                             parent_dir->i_ino, dentry->d_name.name);

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

        err = stamfs_iop_unlink(parent_dir, dentry);
        if (err)
                goto ret_err;

        child_dir->i_size = 0;

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
