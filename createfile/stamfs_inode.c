
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
                ino->i_op = &stamfs_file_iops;
                ino->i_fop = &stamfs_file_fops;
                ino->i_mapping->a_ops = &stamfs_aops;
        }
        else if (S_ISDIR(ino->i_mode)) {
                STAMFS_DBG(DEB_STAM, "inode %ld is a directory\n", ino->i_ino);
                ino->i_op = &stamfs_dir_iops;
                ino->i_fop = &stamfs_dir_fops;
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
 * Update the on-disk copy of the given inode, based on the data in the given
 * VFS inode struct.
 * @param do_sync - 1 if we should perform the disk I/O now and wait for
 *                  its completion, 0 otherwise.
 * @return 0 on success, a negative error code on failure.
 */
int stamfs_inode_write_ino (struct inode *ino, int do_sync)
{
        int err = 0;
        struct super_block *sb = ino->i_sb;
        ino_t ino_num = ino->i_ino;
        struct stamfs_inode_meta_data *stamfs_inode_meta = STAMFS_INODE_META(ino);
        __u32 inode_block_num = stamfs_inode_meta->i_block_num;
        struct buffer_head *ibh = NULL;
        struct stamfs_inode *stamfs_ino = NULL;

        STAMFS_DBG(DEB_STAM, "stamfs: do-writing inode %ld\n", ino_num);

        /* load the inode's block. */
        if (!(ibh = bread(sb->s_dev, inode_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read block %d.\n", inode_block_num);
                err = -EIO;
                goto ret;
        }

        stamfs_ino = (struct stamfs_inode*)((char*)(ibh->b_data));

        /* copy data from the VFS's inode to the on-disk inode. */
        stamfs_ino->i_mode = cpu_to_le16(ino->i_mode);
        stamfs_ino->i_num_links = cpu_to_le16(ino->i_nlink);
        stamfs_ino->i_uid = cpu_to_le32(ino->i_uid);
        stamfs_ino->i_gid = cpu_to_le32(ino->i_gid);
        stamfs_ino->i_atime = cpu_to_le32(ino->i_atime);
        stamfs_ino->i_mtime = cpu_to_le32(ino->i_mtime);
        stamfs_ino->i_ctime = cpu_to_le32(ino->i_ctime);
        stamfs_ino->i_num_blocks = cpu_to_le32(ino->i_blocks);
        stamfs_ino->i_size = cpu_to_le32(ino->i_size);
        /* write this even thought it doesn't change - to be on the safe side. */
        stamfs_ino->i_index_block = cpu_to_le32(stamfs_inode_meta->i_bi_block_num);
        mark_buffer_dirty_inode(ibh, ino);

        /* for a synchronous operation - write the buffer immediately. */
        if (do_sync) {
                ll_rw_block(WRITE, 1, &ibh);
                wait_on_buffer(ibh);
                if (buffer_req(ibh) && !buffer_uptodate(ibh)) {
                        printk("IO error syncing stamfs inode %ld\n", ino_num);
                        err = -EIO;
                        goto ret;
                }
        }

  ret:
        if (ibh)
                brelse(ibh);
        return err;
}

/*
 * Free an existing inode. Free the block index and the inode's block,
 * as well as the inode number.
 */
int stamfs_inode_free_inode(struct inode *ino)
{
        int err = 0;
        struct super_block *sb = ino->i_sb;
        struct stamfs_inode_meta_data *inode_meta = STAMFS_INODE_META(ino);
        int inode_block_num = stamfs_inode_to_block_num(ino);
        int bi_block_num = inode_meta->i_bi_block_num;

        STAMFS_DBG(DEB_STAM, "stamfs: freeing inode %lu\n", ino->i_ino);

        /* if we fail freeing the inode num, we shouldn't release the blocks. */
        err = stamfs_release_inode_num(sb, ino->i_ino);
        if (err < 0)
                goto ret;

        /* if we fail freeing the blocks - a file-system check program will
         * need to reclaim these blocks (which no one points to now). */
        stamfs_release_block(sb, inode_block_num);
        stamfs_release_block(sb, bi_block_num);

        /* all went well... */
        err = 0;

  ret:
        return err;
}

/*
 * free all data blocks of the given inode, making it refer to a
 * 0-length file/directory.
 */
int stamfs_inode_do_truncate(struct inode *ino)
{
        int err = 0;
        struct super_block *sb = ino->i_sb;
        struct stamfs_inode_meta_data *inode_meta = STAMFS_INODE_META(ino);
        int bi_block_num = inode_meta->i_bi_block_num;
        struct buffer_head *bibh = NULL;
        struct stamfs_inode_block_index *stamfs_bi = NULL;
        int ino_size = ino->i_size;
        int block_size;
        int i;
        unsigned int curr_block_num;
        int freed_blocks_count = 0;

        STAMFS_DBG(DEB_STAM,
                   "stamfs: truncating inode %lu, which has %ld blocks\n",
                   ino->i_ino, ino->i_blocks);

        /* read the inode's block index. */
        if (!(bibh = bread(sb->s_dev, bi_block_num, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read inode block index, block %d.\n",
                       bi_block_num);
                err = -EIO;
                goto ret;
        }
        stamfs_bi = (struct stamfs_inode_block_index *)((char *)(bibh->b_data));

        /* free each data block which is fully beyond the inode's data size. */
        /* NOTE: one block might now be "half-truncated" - this is handled   */
        /*       when reading or seeking (assuming this is a regular file).  */
        block_size = sb->s_blocksize;
        i = (ino_size + (block_size - 1)) >> sb->s_blocksize_bits;

        /* TODO - handle page-cache truncation for normal files... */

        STAMFS_DBG(DEB_STAM, "stamfs: freeing from block offset %d\n", i);
        for ( ; i < STAMFS_MAX_BLOCKS_PER_FILE; i++) {
                curr_block_num = stamfs_bi->index[i];
                if (curr_block_num != STAMFS_FREE_BLOCK_MARKER && curr_block_num != 0) {
                        stamfs_bi->index[i] = STAMFS_FREE_BLOCK_MARKER;
                        STAMFS_DBG(DEB_STAM, "stamfs: freeing block %u\n",
                                             curr_block_num);
                        /* if we fail freeing the block - a file-system check
                         * program will need to reclaim these blocks (which
                         * no one points to now). */
                        stamfs_release_block(sb, curr_block_num);
                        freed_blocks_count++;
                }
        }

        STAMFS_DBG(DEB_STAM, "stamfs: freed %d blocks\n", freed_blocks_count);

        mark_buffer_dirty_inode(bibh, ino);

        /* the VFS already handled the update of the _size_ of the inode. */
        ino->i_blocks -= freed_blocks_count;
        ino->i_mtime = ino->i_ctime = CURRENT_TIME;
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
 * free all data blocks of the given inode, making it refer to a
 * 0-length file.
 */
void stamfs_inode_truncate(struct inode *ino)
{
        stamfs_inode_do_truncate(ino);
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
