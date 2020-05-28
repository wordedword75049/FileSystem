
#include "stamfs.h"
#include "stamfs_super.h"
#include "stamfs_iops.h"
#include "stamfs_aops.h"
#include "stamfs_util.h"

/* forward declerations. */
int stamfs_readpage(struct file *, struct page *);
int stamfs_writepage(struct page *);
int stamfs_prepare_write(struct file *, struct page *, unsigned, unsigned);


/*
 * Data structures.
 */

/*
 * We forward all of these operations to functions supplied by the generic
 * part of the VFS's code.
 */
struct address_space_operations stamfs_aops = {
        readpage:       stamfs_readpage,
        writepage:      stamfs_writepage,
        sync_page:      block_sync_page,
        prepare_write:  stamfs_prepare_write,
        commit_write:   generic_commit_write
};

/*
 * Utility functions.
 */

/*
 * Given a buffer head, print its meta-data to the debug log.
 * Used for debugging.
 */
void stamfs_dbg_print_bh(struct buffer_head *bh)
{
        STAMFS_DBG(DEB_STAM, "stamfs: bh:\n");
        STAMFS_DBG(DEB_STAM, "stamfs: b_blocknr - %lu\n", bh->b_blocknr);
        STAMFS_DBG(DEB_STAM, "stamfs: b_rsector - %lu\n", bh->b_rsector);
        STAMFS_DBG(DEB_STAM, "stamfs: b_size - %hu\n", bh->b_size);
        STAMFS_DBG(DEB_STAM, "stamfs: b_count - %d\n", atomic_read(&bh->b_count));
        STAMFS_DBG(DEB_STAM, "stamfs: b_state - %lu\n", bh->b_state);
        STAMFS_DBG(DEB_STAM, "stamfs: b_page - %p\n", bh->b_page);
        STAMFS_DBG(DEB_STAM, "stamfs: b_data - %p\n", bh->b_data);
        STAMFS_DBG(DEB_STAM, "stamfs: b_ino - %ld\n",
                             (bh->b_inode ? (bh->b_inode->i_ino) : -1));
}

/*
 * the aop (address-space operations) functions themselves.
 */

/*
 * Given an inode, find the block number to which a given block offset is
 * mapped.
 * @param ino - the inode for which the block is requested.
 * @param block_offset - the logical position of the block in the file.
 * @param buffer_head - the buffer-head into which we should write the
 *                      matching physical disk information.
 * @param create - used for 'write' operations - if this flag is not 0 and the
 *                 block is not mapped - create a mapping to it, rather then 
 *                 returning an error.
 * @return 0 on success or a negative error code on failure.
 */
int stamfs_get_block(struct inode *ino, long block_offset,
                     struct buffer_head *bh_result, int create)
{
        int err = 0;
        int block_num = -1;

        STAMFS_DBG(DEB_STAM, "stamfs: ino=%ld, block_offset=%ld, create=%d\n",
                             ino->i_ino, block_offset, create);

        err = stamfs_inode_block_offset_to_number(ino, block_offset, &block_num);
        if (err)
                goto ret_err;

        /* the block offset is mapped - set the number in bh_result,
         * and return. */
        if (block_num != -1) {
                bh_result->b_dev = ino->i_dev;
                bh_result->b_blocknr = block_num;
                bh_result->b_state |= (1UL << BH_Mapped);
                STAMFS_DBG(DEB_STAM,
                           "stamfs: found existing block, block_num=%d\n",
                           block_num);
                stamfs_dbg_print_bh(bh_result);
                goto ret;
        }

        /* the block offset is not mapped, and create == 0 - return no block. */
        if (create == 0) {
                STAMFS_DBG(DEB_STAM,
                           "stamfs: block not mapped and create==0, "
                           "returning no block\n");
                goto ret;
        }

        /* block offset not mapped and create != 0 - allocate a new block. */
        block_num = stamfs_alloc_block(ino->i_sb);
        if (block_num == 0) {
                STAMFS_DBG(DEB_STAM, "stamfs: cannot allocate block - "
                                     "no free blocks available\n");
                err = -ENOSPC;
                goto ret_err;
        }

        err = stamfs_inode_map_block_offset_to_number(ino,
                                                      block_offset,
                                                      block_num);
        if (err) {
                STAMFS_DBG(DEB_STAM,
                           "stamfs: failed updating the block mapping\n");
                goto ret_err;
        }

        /* the block is now mapped, and its a new block. */
        bh_result->b_dev = ino->i_dev;
        bh_result->b_blocknr = block_num;
        bh_result->b_state |= (1UL << BH_New) | (1UL << BH_Mapped);

        stamfs_dbg_print_bh(bh_result);

        /* all went well... */
        err = 0;
        goto ret;

  ret_err:
        if (block_num > 0)
                stamfs_release_block(ino->i_sb, block_num);
        /* fall through. */
  ret:
        return err;
}

/* delegate the work to the VFS's page reading function. */
int stamfs_readpage(struct file *filp, struct page *page)
{
        STAMFS_DBG(DEB_STAM, "stamfs: readpage, file=%s\n",
                             filp->f_dentry->d_name.name);
        return block_read_full_page(page, stamfs_get_block);
}

/* delegate the work to the VFS's page writing function. */
int stamfs_writepage(struct page *page)
{
        STAMFS_DBG(DEB_STAM, "stamfs: writepage, page=%lu\n", page->index);
        return block_write_full_page(page, stamfs_get_block);
}

/* delegate the work to the VFS's page prepare writing function. */
int stamfs_prepare_write(struct file *filp, struct page *page,
                         unsigned from, unsigned to)
{
        STAMFS_DBG(DEB_STAM, "stamfs: prepare_write, file=%s, page=%lu\n",
                             filp->f_dentry->d_name.name, page->index);
        return block_prepare_write(page, from, to, stamfs_get_block);
}
