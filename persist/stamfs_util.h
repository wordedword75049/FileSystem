#ifndef STAMFS_UTIL_H_
#define STAMFS_UTIL_H_

/* see snmpfs_main.c */ 
extern unsigned long debug_level; 

int debug_write_proc(struct file *file, const char *buffer,
		     unsigned long count, void *data); 

#ifdef STAMFS_MODULE_DEBUG

#define DEB_NONE          (0UL)       /* nothing */ 
#define DEB_INFO          (1UL << 0)  /* misc */ 
#define DEB_STAM          (1UL << 1)  /* file-system related calls */ 
#define DEB_INIT          (1UL << 2)  /* init and cleanup */ 
#define DEB_ANY           (~0UL)      /* anything */ 

#define STAMFS_DBG(level, msg, args...) do {                                  \
        if ((debug_level & level))                                        \
                printk(/* FIXME: level here */                            \
                       "[%s] " msg , __FUNCTION__ , ##args );             \
} while (0)

#else /* !defined(STAMFS_MODULE_DEBUG) */ 

#define STAMFS_DBG(level, msg, args...) do { } while (0)

#endif /* STAMFS_MODULE_DEBUG */ 

#endif /* STAMFS_UTIL_H_ */
