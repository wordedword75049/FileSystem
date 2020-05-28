

#include <linux/slab.h>

#include "stamfs_iops.h"

#include "stamfs.h"
#include "stamfs_super.h"
#include "stamfs_inode.h"
#include "stamfs_dir.h"
#include "stamfs_iops.h"
#include "stamfs_util.h"

/*
 * Data structures.
 */

struct inode_operations stamfs_dir_iops = {
        lookup:         stamfs_iop_lookup,
};


/*
 * Utility functions.
 */

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
