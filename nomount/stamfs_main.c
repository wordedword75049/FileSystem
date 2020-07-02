
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
#include <linux/proc_fs.h> 


#include "stamfs_util.h"

#define VERSION_STRING "stamfs_0.01"

/* general module information. */
MODULE_AUTHOR("guy keren <choo@actcom.co.il>");
MODULE_DESCRIPTION("STAM a file system");
MODULE_LICENSE("GPL");

/* change this to set the default debug level. */
unsigned long debug_level = DEB_ANY; /* full debug. */

/* module arguments. */
/* 'l' - defines the log level (see values in stamfs_utils.h). */
MODULE_PARM(debug_level, "l"); 
MODULE_PARM_DESC(debug_level, "set to a value larger than 0 for debugging."); 

#define PROC_MODULE_NAME "stamfs"

/*
 * An empty sper-block reading function.
 */
struct super_block *stamfs_read_super (struct super_block *sb, void *opt,
                                       int silent)
{
    /* what are you talking about?? we have no code yet - fail fail fail!! */
    printk(KERN_INFO "stamfs_read_super: i am not yet implemented.\n"); 
    return NULL;
}

/*
 * The top-level file-system struct, as mandated by the VFS.
 */
struct file_system_type stamfs_fstype = {
        "stamfs",               /* name of this file-system type.            */
        FS_REQUIRES_DEV,        /* this is a disk-based file-system.         */
        stamfs_read_super,      /* function that reads a STAMFS super-block. */
        NULL                    /* module owning this file-system.           */
};

/*
 * Handle file-system global functions.
 */

static int init_fs(void)
{
        return register_filesystem(&stamfs_fstype);
}

static void cleanup_fs(void)
{
        unregister_filesystem(&stamfs_fstype);
}


/*
 * Handling of registration under the /proc file system, for debug purposes.
 */

struct proc_dir_entry* dir; 

static void init_debug(void)
{
        struct proc_dir_entry* res; 

        if (!(dir = proc_mkdir(PROC_MODULE_NAME, NULL))) {
                printk(KERN_WARNING "proc_mkdir failed\n"); 
                return; 
        }
        
        dir->owner = THIS_MODULE; 
        
        if (!(res = create_proc_entry("debug", 0644, dir))) {
                printk("create_proc_entry 'debug' failed\n"); 
                goto out1; 
        }

        res->write_proc = debug_write_proc; 
        res->owner = THIS_MODULE; 

        return; 
        
 out1: 
        remove_proc_entry(PROC_MODULE_NAME, NULL); 
}

static void cleanup_debug(void)
{
        remove_proc_entry("debug", dir); 
        remove_proc_entry(PROC_MODULE_NAME, NULL); 
}

/*
 * Module initialization - initialize our debug support, and register
 * the file-system.
 */
int init_module (void)
{
        STAMFS_DBG(DEB_INIT, "stamfs initializing...\n"); 

        init_debug(); 

        if (init_fs()) {
                printk(KERN_ERR "%s not loaded - failed registering FS.\n",
                                VERSION_STRING); 
                return -1;
        }

        printk(KERN_INFO "%s loaded.\n", VERSION_STRING); 
        return 0;
}

/*
 * Module cleanup - un-register the file-system, and cleanup debug support.
 */
void cleanup_module (void)
{
        STAMFS_DBG(DEB_INIT, "stamfs cleaning up...\n");

        cleanup_fs();

        cleanup_debug(); 

        printk(KERN_INFO "%s unloaded.\n", VERSION_STRING); 
}
