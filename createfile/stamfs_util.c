
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/proc_fs.h> 
#include <linux/module.h> 
#include <asm/uaccess.h>

#include "stamfs_util.h"

/*
 * Support changing the debug-level of the STAMFS module, via the proc
 * file-system.
 */
int debug_write_proc(struct file *file, const char *buffer,
		     unsigned long count, void *data)
{
	size_t bufsz = 20; 
	char str[bufsz]; 
	unsigned long old_debug_level; 
	int ret; 
	
	MOD_INC_USE_COUNT; 

	ret = -E2BIG; 
	if (count > bufsz)
		goto done; 

	ret = -EFAULT; 
	if (copy_from_user(str, buffer, count))
		goto done; 

	old_debug_level  = debug_level; 
	debug_level = simple_strtoul(str, NULL, 0); 

	STAMFS_DBG(DEB_ANY, "changing debug level: 0x%lx -> 0x%lx\n", 
	           old_debug_level, debug_level); 

	ret = count; 
 done: 
	MOD_DEC_USE_COUNT; 
	return ret; 
}
