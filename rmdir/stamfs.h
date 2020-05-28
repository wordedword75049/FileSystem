

#ifndef STAMFS_H
#define STAMFS_H

/*
 * Types, macros and functions that define the structure of a STAM file-system.
 * To be used both from the kernel module, and from user-space utilities
 * (e.g. mkstamfs).
 */

/*
 * exported types.
 */

/* every file-system must have a magic-number in its super-block. */
#define STAMFS_SUPER_MAGIC  0x1013f5ee
#define STAMFS_BLOCK_SIZE       1024

/* hard-coded block numbers for storing super-block, inode index, etc. */
#define STAMFS_SUPER_BLOCK_NUM  1
#define STAMFS_INODES_BLOCK_NUM (STAMFS_SUPER_BLOCK_NUM+1)
#define STAMFS_FREE_LIST_BLOCK_NUM   (STAMFS_INODES_BLOCK_NUM+1)
#define STAMFS_LAST_HARDCODED_BLOCK_NUM (STAMFS_FREE_LIST_BLOCK_NUM)

/* hard-coded root inode number. */
#define STAMFS_ROOT_INODE_NUM   1

/* limits. */
#define STAMFS_MAX_INODE_NUM ((STAMFS_BLOCK_SIZE / 4) + 1)
#define STAMFS_MAX_BLOCK_NUMS_PER_BLOCK (STAMFS_BLOCK_SIZE / 4)
#define STAMFS_MAX_BLOCKS_PER_FILE STAMFS_MAX_BLOCK_NUMS_PER_BLOCK
#define STAMFS_MAX_FNAME_LEN    16

/* special markers inside lists. */
#define STAMFS_FREE_BLOCK_MARKER        (~(__u32)0)
#define STAMFS_FREE_DIR_REC_MARKER      (~(__u32)0)


/* types with given sizes, to make a STAMFS more portable. */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>

typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;

#endif

/* data structures used to store STAMFS constructs on disk blocks. */
struct stamfs_super_block {
        __u32 s_magic;
        __u32 s_inodes_count;
        __u32 s_blocks_count;
        __u32 s_free_inodes_count;
        __u32 s_free_blocks_count;
        __u32 s_free_list_block_num;
        __u32 s_highest_used_block_num;
};

struct stamfs_inode_index {
        __u32 index[STAMFS_MAX_INODE_NUM-1];
};

struct stamfs_free_list_index {
        __u32 index[STAMFS_MAX_BLOCK_NUMS_PER_BLOCK];
};


struct stamfs_inode {
        __u16 i_mode;
        __u16 i_num_links;
        __u32 i_uid;
        __u32 i_gid;
        __u32 i_size;
        __u32 i_atime;
        __u32 i_mtime;
        __u32 i_ctime;
        __u32 i_num_blocks;
        __u32 i_index_block;
};

struct stamfs_inode_block_index {
        __u32 index[STAMFS_MAX_BLOCKS_PER_FILE];
};

#define STAMFS_DIR_REC_FTYPE_UNKNOWN    0
#define STAMFS_DIR_REC_FTYPE_DIR        1
#define STAMFS_DIR_REC_FTYPE_FILE       2

struct stamfs_dir_rec {
        __u32 dr_ino;
        __u8  dr_name_len;
        __u8  dr_ftype;
        char  dr_name[STAMFS_MAX_FNAME_LEN];
};

#endif /* STAMFS_H */
