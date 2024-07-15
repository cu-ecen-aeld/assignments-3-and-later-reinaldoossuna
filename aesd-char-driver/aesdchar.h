/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#define AESD_DEBUG 1 // Remove comment on this line to enable debug

#undef PDEBUG /* undef it, just in case */
#ifdef AESD_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "aesdchar: " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#include <linux/cdev.h>
#include <linux/mutex.h>

#include "aesd-circular-buffer.h"

int aesd_open(struct inode *inode, struct file *filp);
void aesd_cleanup_module(void);
int aesd_init_module(void);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos);
int aesd_release(struct inode *inode, struct file *filp);
loff_t aesd_llseek(struct file *filp, loff_t off, int whence);
long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

struct aesd_dev {
  struct cdev cdev; /* Char device structure      */

  struct aesd_circular_buffer buffer;
  struct mutex buffer_mutex;

  struct aesd_buffer_entry partial_entry;
};

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
