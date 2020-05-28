

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


#define STAMFS_META(sb) ((struct stamfs_meta_data *)((sb)->u.generic_sbp))

/*
 * Forward declerations.
 */
void stamfs_read_inode (struct inode *);
void stamfs_clear_inode (struct inode *);
void stamfs_put_super (struct super_block *);
void stamfs_write_super (struct super_block *);
int stamfs_statfs (struct super_block *, struct statfs *);

 
/* The super-block operations structure. */
static struct super_operations stamfs_super_ops = {
        read_inode: stamfs_read_inode,
        clear_inode: stamfs_clear_inode,
        put_super: stamfs_put_super,
        write_super: stamfs_write_super,
        statfs: stamfs_statfs,
};


/*
 * Internal data structures.
 */

struct stamfs_meta_data {
        struct buffer_head *s_sbh;
        struct stamfs_super_block *s_stamfs_sb;
        struct buffer_head *s_iibh;
        struct stamfs_inode_index *s_stamfs_ii;
        struct buffer_head *s_flbh;
        struct stamfs_free_list_index *s_stamfs_fl;
};


/*
 * Utility functions.
 */

/*
 * Allocates a free block number.
 * returns 0 if no free numbers are available.
 */
int stamfs_alloc_block(struct super_block *sb)
{
        kdev_t dev = sb->s_dev;
        struct stamfs_meta_data* stamfs_meta = STAMFS_META(sb);
        struct stamfs_super_block* stamfs_sb = stamfs_meta->s_stamfs_sb;
        struct stamfs_free_list_index *stamfs_fl = stamfs_meta->s_stamfs_fl;
        struct buffer_head *sbh = stamfs_meta->s_sbh;
        struct buffer_head *flbh = stamfs_meta->s_flbh;
        int block_num = 0;
        int i;

        STAMFS_DBG(DEB_INIT, "stamfs: allocating block, dev='%d:%d'\n",
                             major(dev), minor(dev));

        lock_super(sb);

        if (stamfs_sb->s_free_blocks_count == 0) {
                STAMFS_DBG(DEB_STAM, "stamfs: no more free blocks.\n");
                goto ret;
        }

        /* first, scan the list of free blocks. */
        if (stamfs_fl->index[0] != 0) {
                STAMFS_DBG(DEB_STAM, "stamfs: scanning the free list...\n");
                for (i=0; i < STAMFS_MAX_BLOCK_NUMS_PER_BLOCK; ++i)
                        if (stamfs_fl->index[i] == 0 || stamfs_fl->index[i] != STAMFS_FREE_BLOCK_MARKER)
                                break;
                /* so, did we find a free block? */
                if (stamfs_fl->index[i] != 0 && stamfs_fl->index[i] != STAMFS_FREE_BLOCK_MARKER) {
                        STAMFS_DBG(DEB_STAM,
                                   "stamfs: allocating from the free list, "
                                   "pos=%d\n",
                                   i);
                        block_num = stamfs_fl->index[i];
                        if (i+1 == STAMFS_MAX_BLOCK_NUMS_PER_BLOCK || stamfs_fl->index[i+1] == 0) {
                                STAMFS_DBG(DEB_STAM,
                                           "stamfs: was last, shrinking free "
                                           "list to size %d\n",
                                           i);
                                stamfs_fl->index[i] = 0;
                        }
                        else
                                stamfs_fl->index[i] = STAMFS_FREE_BLOCK_MARKER;
                        mark_buffer_dirty(flbh);
                }
        }
        /* there was none on the free list - allocate past the highest used. */
        if (block_num == 0) {
                block_num = ++stamfs_sb->s_highest_used_block_num;
                /* no need to mark sbh as dirty - we do that below anyway. */
        }

        stamfs_sb->s_free_blocks_count--;
        mark_buffer_dirty(sbh);
        sb->s_dirt = 1;

        unlock_super(sb);

        STAMFS_DBG(DEB_STAM, "stamfs: allocated block number %d\n", block_num);

  ret:
        return block_num;
}

/*
 * Frees a previously allocated block number.
 * returns 0 on success, a negative error code on failure.
 */
int stamfs_release_block(struct super_block *sb, int block_num)
{
        kdev_t dev = sb->s_dev;
        struct stamfs_meta_data* stamfs_meta = STAMFS_META(sb);
        struct stamfs_super_block* stamfs_sb = stamfs_meta->s_stamfs_sb;
        struct stamfs_free_list_index *stamfs_fl = stamfs_meta->s_stamfs_fl;
        struct buffer_head *sbh = stamfs_meta->s_sbh;
        struct buffer_head *flbh = stamfs_meta->s_flbh;
        int i;

        STAMFS_DBG(DEB_INIT, "stamfs: freeing block %d, dev='%d:%d'\n",
                             block_num, major(dev), minor(dev));

        /* sanity check - don't allow freeing any of the mandatory blocks. */
        if (block_num <= STAMFS_LAST_HARDCODED_BLOCK_NUM) {
                printk("stamfs: trying to free at or below "
                       "mandatory block %d.\n",
                       STAMFS_LAST_HARDCODED_BLOCK_NUM);
                BUG();
        }

        lock_super(sb);

        /* this is the highest used block number - no need to use the free list. */
        if (block_num == stamfs_sb->s_highest_used_block_num) {
                STAMFS_DBG(DEB_STAM, "stamfs: freed highest used block.\n");
                stamfs_sb->s_highest_used_block_num--;
                /* no need to mark sbh as dirty - we do that below anyway. */
        }
        else {
                /* add it to the free list. */

                /* scan the list of free blocks for an empty slot. */
                for (i=0; i < STAMFS_MAX_BLOCK_NUMS_PER_BLOCK; ++i)
                        if (stamfs_fl->index[i] == 0 || stamfs_fl->index[i] == STAMFS_FREE_BLOCK_MARKER)
                                break;
                /* so, did we find a free slot? */
                if (i >= STAMFS_MAX_BLOCK_NUMS_PER_BLOCK) {
                        printk("stamfs: free blocks list is full!");
                        BUG();
                }

                if (stamfs_fl->index[i] == 0) {
                        STAMFS_DBG(DEB_STAM,
                                   "stamfs: free list increased, "
                                   "new len - %d.\n",
                                   i+1);
                        if (i+1 < STAMFS_MAX_BLOCK_NUMS_PER_BLOCK)
                                stamfs_fl->index[i+1] = 0;
                }
                STAMFS_DBG(DEB_STAM, "stamfs: adding to free list at pos %d\n",
                                     i);
                stamfs_fl->index[i] = block_num;
                mark_buffer_dirty(flbh);
        }

        stamfs_sb->s_free_blocks_count++;
        mark_buffer_dirty(sbh);
        sb->s_dirt = 1;

        unlock_super(sb);

        STAMFS_DBG(DEB_STAM, "stamfs: block %d freed\n", block_num);

        return 0;
}

/*
 * Allocates a free inode number, mapping it to the given block number.
 * returns 0 if no free numbers are available.
 */
ino_t stamfs_alloc_inode_num(struct super_block *sb, int block_num)
{
        kdev_t dev = sb->s_dev;
        struct stamfs_meta_data* stamfs_meta = STAMFS_META(sb);
        struct stamfs_super_block* stamfs_sb = stamfs_meta->s_stamfs_sb;
        struct stamfs_inode_index *stamfs_ii = stamfs_meta->s_stamfs_ii;
        struct buffer_head *sbh = stamfs_meta->s_sbh;
        struct buffer_head *iibh = stamfs_meta->s_iibh;
        ino_t ino_num = 0;
        int i;

        STAMFS_DBG(DEB_INIT,
                   "stamfs: allocating inode, block=%d, dev='%d:%d'\n",
                   block_num, major(dev), minor(dev));

        lock_super(sb);

        if (stamfs_sb->s_free_inodes_count == 0) {
                STAMFS_DBG(DEB_STAM, "stamfs: no more free inodes.\n");
                goto ret;
        }

        /* scan the list, find the first free inode. */
        for (i=0; i < STAMFS_MAX_INODE_NUM - 1; ++i)
                if (stamfs_ii->index[i] == 0)
                        break;

        ino_num = i+1;
        stamfs_ii->index[i] = block_num;
        mark_buffer_dirty(iibh);
        stamfs_sb->s_free_inodes_count--;
        mark_buffer_dirty(sbh);
        sb->s_dirt = 1;

        STAMFS_DBG(DEB_STAM, "stamfs: allocated inode number '%lu'\n", ino_num);

  ret:
        unlock_super(sb);
        return ino_num;
}

/*
 * Frees a previously allocated inode number.
 */
int stamfs_release_inode_num(struct super_block *sb, ino_t ino_num)
{
        kdev_t dev = sb->s_dev;
        struct stamfs_meta_data* stamfs_meta = STAMFS_META(sb);
        struct stamfs_super_block* stamfs_sb = stamfs_meta->s_stamfs_sb;
        struct stamfs_inode_index *stamfs_ii = stamfs_meta->s_stamfs_ii;
        struct buffer_head *sbh = stamfs_meta->s_sbh;
        struct buffer_head *iibh = stamfs_meta->s_iibh;

        STAMFS_DBG(DEB_INIT, "stamfs: freeing inode %lu, dev='%d:%d'\n",
                             ino_num, major(dev), minor(dev));

        /* sanity check - don't allow freeing the root inode. */
        if (ino_num == STAMFS_ROOT_INODE_NUM) {
                printk("stamfs: trying to free the root inode %d.\n",
                       STAMFS_ROOT_INODE_NUM);
                BUG();
        }

        lock_super(sb);

        /* mark this inode as free. */
        stamfs_ii->index[ino_num-1] = 0;
        mark_buffer_dirty(iibh);
        stamfs_sb->s_free_inodes_count++;
        mark_buffer_dirty(sbh);
        sb->s_dirt = 1;

        unlock_super(sb);

        STAMFS_DBG(DEB_STAM, "stamfs: freed inode number '%lu'\n", ino_num);

        return 0;
}

/*
 * Finds the block that the given inode's info is stored in.
 * returns the block number, or 0 on error.
 */
unsigned long stamfs_inode_to_block_num(struct inode *ino)
{
        unsigned long ino_num = ino->i_ino;
        unsigned long block_num = 0;
        struct stamfs_inode_index *stamfs_ii;

        if (ino_num < 1 || ino_num > STAMFS_MAX_INODE_NUM) {
                STAMFS_DBG(DEB_STAM,
                           "stamfs: inode number '%lu' is out of range\n",
                           ino_num);
                return 0;
        }

        stamfs_ii = STAMFS_META(ino->i_sb)->s_stamfs_ii;
        block_num = le32_to_cpu(stamfs_ii->index[ino_num-1]);
        STAMFS_DBG(DEB_STAM, "stamfs: inode number '%lu' is on block %lu\n",
                             ino_num, block_num);

        return block_num;
}


/*
 * super-block operations.
 */

/* this one is not static, since it's used from outside this file. */
struct super_block *stamfs_read_super (struct super_block *sb, void *opt, int silent)
{
        int err = -1;
        struct inode *root_ino = NULL;
        kdev_t dev = sb->s_dev;
        struct buffer_head *bh = NULL;
        struct buffer_head *iibh = NULL;
        struct buffer_head *flbh = NULL;
        struct stamfs_super_block *stamfs_sb = NULL;
        struct stamfs_inode_index *stamfs_ii = NULL;
        struct stamfs_free_list_index *stamfs_fl = NULL;
        struct stamfs_meta_data *stamfs_meta = NULL;

        MOD_INC_USE_COUNT;

        STAMFS_DBG(DEB_INIT, "stamfs: reading superblock, dev='%d:%d'\n",
                             major(dev), minor(dev));

        /* set the device's block size to our desired block size. */
        if (set_blocksize(dev, STAMFS_BLOCK_SIZE)) {
                printk("stamfs: can't set device's block size to %d.\n",
                       STAMFS_BLOCK_SIZE);
                goto ret_err;
        }

        /* read in the super-block data, inode index and free list from disk. */
        if (!(bh = bread(sb->s_dev, STAMFS_SUPER_BLOCK_NUM, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read superblock.\n");
                goto ret_err;
        }
        stamfs_sb = (struct stamfs_super_block *)((char *)(bh->b_data));

        if (!(iibh = bread(sb->s_dev, STAMFS_INODES_BLOCK_NUM, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read inodes index block.\n");
                goto ret_err;
        }
        stamfs_ii = (struct stamfs_inode_index *)((char *)(iibh->b_data));

        if (!(flbh = bread(sb->s_dev, STAMFS_FREE_LIST_BLOCK_NUM, STAMFS_BLOCK_SIZE))) {
                printk("stamfs: unable to read free list block.\n");
                goto ret_err;
        }
        stamfs_fl = (struct stamfs_free_list_index *)((char *)(flbh->b_data));


        /* check that the device indeed contains a STAMFS file system. */
        if (le32_to_cpu(stamfs_sb->s_magic) != STAMFS_SUPER_MAGIC) {
                printk("stamfs: bad super-block magic (0x%x) on dev %s.\n",
                       stamfs_sb->s_magic, bdevname(dev));
                goto ret_err;
        }

        /* initialize our meta-data. */
        stamfs_meta = kmalloc(sizeof(struct stamfs_meta_data), GFP_KERNEL);
        if (!stamfs_meta) {
                printk("stamfs: not enough memory to allocate meta struct.\n");
                goto ret_err;
        }
        stamfs_meta->s_sbh = bh;
        stamfs_meta->s_stamfs_sb = stamfs_sb;
        stamfs_meta->s_iibh = iibh;
        stamfs_meta->s_stamfs_ii = stamfs_ii;
        stamfs_meta->s_flbh = flbh;
        stamfs_meta->s_stamfs_fl = stamfs_fl;

        /* initialize the VFS's super-block struct. */
        sb->s_blocksize = STAMFS_BLOCK_SIZE;
        sb->s_blocksize_bits = 10;
        sb->s_maxbytes = (STAMFS_BLOCK_SIZE / 4) * STAMFS_BLOCK_SIZE;
        sb->s_magic = STAMFS_SUPER_MAGIC;
        sb->s_op = &stamfs_super_ops;
        sb->u.generic_sbp = stamfs_meta;

        /* load the root inode - every FS scan starts from it. */
        root_ino = iget(sb, STAMFS_ROOT_INODE_NUM);
        if (!root_ino) {
                STAMFS_DBG(DEB_INIT, "stamfs: reading root inode failed\n");
                goto ret_err;
        }
        sb->s_root = d_alloc_root(root_ino);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        if (root_ino)
                iput(root_ino);
        if (stamfs_meta)
                kfree(stamfs_meta);
        if (bh)
                brelse(bh);
        if (iibh)
                brelse(iibh);
        if (flbh)
                brelse(flbh);
        MOD_DEC_USE_COUNT;
  ret:
        return err ? NULL : sb;
}

void stamfs_put_super (struct super_block *sb)
{
        struct stamfs_meta_data *stamfs_meta = STAMFS_META(sb);

        STAMFS_DBG(DEB_STAM,
                   "stamfs: releasing (umount) superblock, dev='%d:%d'\n",
                   major(sb->s_dev), minor(sb->s_dev));

        /* free memory used by the stamfs-portion of the super-block. */
        if (!stamfs_meta)
                BUG();

        brelse(stamfs_meta->s_sbh);
        brelse(stamfs_meta->s_iibh);
        brelse(stamfs_meta->s_flbh);
        kfree(stamfs_meta);

        MOD_DEC_USE_COUNT;
}

void stamfs_write_super (struct super_block *sb)
{
        STAMFS_DBG(DEB_STAM,
                   "stamfs: writing superblock, dev='%d:%d'\n",
                   major(sb->s_dev), minor(sb->s_dev));

        /* nothing to do - the super-block is stored in buffers, which */
        /* get written to disk by the system anyway, and get synced    */
        /* immediately by the VFS anyway when it needs umount this FS. */
        sb->s_dirt = 0;
}

/* read the given inode into memory. */
void stamfs_read_inode (struct inode *ino)
{
        unsigned long block_num = 0;

        STAMFS_DBG(DEB_STAM, "stamfs: reading inode %ld\n", ino->i_ino);

        /* find the inode's block number. */
        block_num = stamfs_inode_to_block_num(ino);
        if (block_num == 0) {
                STAMFS_DBG(DEB_STAM, "stamfs: cannot find block for inode %ld\n", ino->i_ino);
                goto ret_err;
        }

        stamfs_inode_read_ino(ino, block_num);

        return;

  ret_err:
        make_bad_inode(ino);
}

/* clear any extra memory used by this inode memory object. */
void stamfs_clear_inode (struct inode *ino)
{
        STAMFS_DBG(DEB_STAM, "stamfs: the VFS cleared inode %ld\n", ino->i_ino);
        stamfs_inode_clear(ino);
}

int stamfs_statfs (struct super_block *sb, struct statfs *stat)
{
        struct stamfs_super_block *stamfs_sb = NULL;

        STAMFS_DBG(DEB_STAM,
                   "stamfs: fs-stating superblock, dev='%d:%d'\n",
                   major(sb->s_dev), minor(sb->s_dev));

        stamfs_sb = STAMFS_META(sb)->s_stamfs_sb;
        memset(stat, 0, sizeof(struct statfs));
        stat->f_type = STAMFS_SUPER_MAGIC;
        stat->f_bsize = sb->s_blocksize;
        stat->f_blocks = le32_to_cpu(stamfs_sb->s_blocks_count);
        stat->f_bfree = le32_to_cpu(stamfs_sb->s_free_blocks_count);
        stat->f_bavail = stat->f_bfree;
        stat->f_files = le32_to_cpu(stamfs_sb->s_inodes_count);
        stat->f_ffree = le32_to_cpu(stamfs_sb->s_free_inodes_count);
        stat->f_namelen = STAMFS_MAX_FNAME_LEN;

        printk("stamfs: f_blocks=%lu, f_bfree=%lu, f_bavail=%lu\n",
               stat->f_blocks, stat->f_bfree, stat->f_bavail);

        return 0;
}
