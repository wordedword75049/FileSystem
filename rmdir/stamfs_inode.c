

#include <linux/module.h>
#include <linux/version.h>
#include <linux/config.h>
#include <linux/sys.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/fs.h> 
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/locks.h>

#include "stamfs.h"
#include "stamfs_util.h"
#include "stamfs_super.h"
#include "stamfs_inode.h"
#include "stamfs_iops.h"
#include "stamfs_fops.h"
#include "stamfs_aops.h"

/*
 * Given a VFS inode and the inode's block on disk, read the inode's contents
 * into memory. The inode number is supplied inside the VFS inode struct.
 * @return 0 on success, a negative error code on failure.
 */
int stamfs_inode_read_ino (struct inode *ino, unsigned long block_num)
{
        int err = -ENOMEM;
        struct super_block *sb = ino->i_sb;
        struct buffer_head *ibh = NULL;
        struct stamfs_inode *stamfs_ino = NULL;
        unsigned long bi_block_num = 0;
        struct stamfs_inode_meta_data *stamfs_inode_meta = NULL;


        STAMFS_DBG(DEB_STAM, "stamfs: do-reading inode %ld\n", ino->i_ino);

        /* read the inode's block from disk. */
        if (!(ibh = bread(sb->s_dev, block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read inode block %lu.\n", block_num);
                goto ret_err;
        }
        stamfs_ino = (struct stamfs_inode *)((char *)(ibh->b_data));

        bi_block_num = le32_to_cpu(stamfs_ino->i_index_block);
        STAMFS_DBG(DEB_STAM, "stamfs: inode %ld, index_block_num=%lu\n",
                             ino->i_ino, bi_block_num);

        /* init the inode's meta data. */
        stamfs_inode_meta = kmalloc(sizeof(struct stamfs_inode_meta_data),
                                    GFP_KERNEL);
        if (!stamfs_inode_meta) {
                printk("stamfs: not enough memory to allocate inode meta struct.\n");
                goto ret_err;
        }
        stamfs_inode_meta->i_block_num = block_num;
        stamfs_inode_meta->i_bi_block_num = bi_block_num;

        ino->i_mode = le16_to_cpu(stamfs_ino->i_mode);
        ino->i_nlink = le16_to_cpu(stamfs_ino->i_num_links);
        ino->i_size = le32_to_cpu(stamfs_ino->i_size);
        ino->i_blksize = STAMFS_BLOCK_SIZE;
        ino->i_blkbits = 10;
        ino->i_blocks = le32_to_cpu(stamfs_ino->i_num_blocks);
        ino->i_uid = le32_to_cpu(stamfs_ino->i_uid);
        ino->i_gid = le32_to_cpu(stamfs_ino->i_gid);
        ino->i_atime = le32_to_cpu(stamfs_ino->i_atime);
        ino->i_mtime = le32_to_cpu(stamfs_ino->i_mtime);
        ino->i_ctime = le32_to_cpu(stamfs_ino->i_ctime);
        ino->i_attr_flags = 0;
        ino->u.generic_ip = stamfs_inode_meta;

        /* set the inode operations structs. */
        if (S_ISREG(ino->i_mode)) {
                STAMFS_DBG(DEB_STAM, "inode %ld is a regular file\n", ino->i_ino);
                /* we're not handling files yet. */
                ino->i_op = (struct inode_operations *)NULL;
                ino->i_fop = (struct file_operations *)NULL;
                ino->i_mapping->a_ops = (struct address_space_operations *) NULL;
        }
        else if (S_ISDIR(ino->i_mode)) {
                STAMFS_DBG(DEB_STAM, "inode %ld is a directory\n", ino->i_ino);
                ino->i_op = &stamfs_dir_iops;
                ino->i_fop = &stamfs_dir_fops;
                /* we're not handling directory reading yet. */
                ino->i_mapping->a_ops = &stamfs_aops;
        }

        STAMFS_DBG(DEB_STAM,
                   "stamfs: inode %ld: i_mode=%o, i_nlink=%d, "
                   "i_uid=%d, i_gid=%d\n",
                   ino->i_ino, ino->i_mode, ino->i_nlink,
                   ino->i_uid, ino->i_gid);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        /* mark the inode to be invalid. */
        make_bad_inode(ino);
        if (stamfs_inode_meta)
                kfree(stamfs_inode_meta);
  ret:
        if (ibh)
                brelse(ibh);
        return err;
}

/*
 * Clear any dynamically-allocated resources used by us for the given
 * VFS inode struct.
 */
void stamfs_inode_clear (struct inode *ino)
{
        struct stamfs_inode_meta_data *stamfs_inode_meta = STAMFS_INODE_META(ino);

        /* free memory used by this inode, on behalf of stamfs. */
        kfree(stamfs_inode_meta);
        ino->u.generic_ip = NULL;
}
