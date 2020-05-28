

#ifndef STAMFS_AOPS_H
#define STAMFS_AOPS_H

/*
 * Functions dealing with address-space mapping - used by the page-cache.
 */

#include <linux/mm.h>

/*
 * This struct contains the address-space operations used to read and write
 * data pages (which are contents of files in STAMFS).
 */
extern struct address_space_operations stamfs_aops;

#endif /* STAMFS_AOPS_H */
