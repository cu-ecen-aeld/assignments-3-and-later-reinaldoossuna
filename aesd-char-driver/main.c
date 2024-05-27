/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include "aesdchar.h"
#include <linux/fs.h> // file_operations
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Reinaldo Ossuna");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp);
void aesd_cleanup_module(void);
int aesd_init_module(void);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos);
int aesd_release(struct inode *inode, struct file *filp);

int aesd_open(struct inode *inode, struct file *filp) {
  PDEBUG("open");

  /* add device information to other method */
  struct aesd_dev *dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
  filp->private_data = (void *)dev;

  return 0;
}

int aesd_release(struct inode *inode, struct file *filp) {
  PDEBUG("release");
  /**
   * TODO: handle release
   */
  return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos) {
  ssize_t retval = 0;
  PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

  struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
  if (mutex_lock_interruptible(&dev->buffer_mutex)) {
    return -EINTR;
  }

  size_t entry_offset;
  struct aesd_buffer_entry *entry =
      aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos,
                                                      &entry_offset);
  if (!entry) {
    retval = 0;
    goto out;
  }

  PDEBUG("Read: %.*s", (int)entry->size, entry->buffptr);
  if (copy_to_user(buf, entry->buffptr, entry->size)) {
    retval = -EFAULT;
    goto out;
  }

  *f_pos += entry->size;
  retval = entry->size;

out:
  mutex_unlock(&dev->buffer_mutex);
  return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos) {
  ssize_t retval = -ENOMEM;
  PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

  struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
  if (mutex_lock_interruptible(&dev->buffer_mutex)) {
    return -EINTR;
  }

  // if no partial entry, than buffer_size = count
  size_t buffer_size = count + dev->partial_entry.size;
  char *buffptr = (char *)kmalloc(buffer_size, GFP_KERNEL);
  if (!buffptr) {
    goto out;
  }

  // give space to partial buffer if exists
  if (copy_from_user(buffptr + dev->partial_entry.size, buf, count)) {
    retval = -EFAULT;
    kfree(buffptr);
    goto out;
  }

  /*
  ** if there's a partial entry
  ** copy partial entry to new buffer
  ** free partial entry buffer
  ** reset partial_entry state
  */
  if (dev->partial_entry.buffptr != NULL) {
    memcpy(buffptr, dev->partial_entry.buffptr, dev->partial_entry.size);
    PDEBUG("Free partial write %.*s", (int)dev->partial_entry.size,
           dev->partial_entry.buffptr);
    kfree(dev->partial_entry.buffptr);

    // reset state
    dev->partial_entry.buffptr = NULL;
    dev->partial_entry.size = 0;
  }

  struct aesd_buffer_entry entry = {//
                                    .buffptr = buffptr,
                                    .size = buffer_size};

  /*
   * if is not a partial entry
   * push to circular buffer
   * */
  if (buffptr[buffer_size - 1] == '\n') {

    PDEBUG("Wrote: %.*s", (int)buffer_size, buffptr);
    struct aesd_buffer_entry old_entry =
        aesd_circular_buffer_add_entry(&dev->buffer, &entry);
    if (old_entry.buffptr) {
      PDEBUG("Free overwritten: %.*s", (int)old_entry.size, old_entry.buffptr);
      kfree(old_entry.buffptr);
    }
  } else { // current entry is a partial
    dev->partial_entry = entry;
    PDEBUG("Partial write: %.*s", (int)entry.size, entry.buffptr);
  }

  *f_pos += buffer_size;
  retval = count;

out:
  mutex_unlock(&dev->buffer_mutex);
  return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence) {
  struct aesd_dev *dev = filp->private_data;
  loff_t newpos;

  switch (whence) {
  case 0: /* SEEK_SET */
    newpos = off;
    break;

  case 1: /* SEEK_CUR */
    newpos = filp->f_pos + off;
    break;

  case 2: /* SEEK_END */
    /* newpos = dev->size + off; */
    return -EINVAL;
    break;

  default: /* can't happen */
    return -EINVAL;
  }

  if (newpos < 0)
    return -EINVAL;
  PDEBUG("seek pos to: %lld", newpos);
  filp->f_pos = newpos;
  return newpos;
}

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .llseek = aesd_llseek,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev) {
  int err, devno = MKDEV(aesd_major, aesd_minor);

  cdev_init(&dev->cdev, &aesd_fops);
  dev->cdev.owner = THIS_MODULE;
  dev->cdev.ops = &aesd_fops;
  err = cdev_add(&dev->cdev, devno, 1);
  if (err) {
    printk(KERN_ERR "Error %d adding aesd cdev", err);
  }
  return err;
}

int aesd_init_module(void) {
  dev_t dev = 0;
  int result;
  result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
  aesd_major = MAJOR(dev);
  if (result < 0) {
    printk(KERN_WARNING "Can't get major %d\n", aesd_major);
    return result;
  }
  memset(&aesd_device, 0, sizeof(struct aesd_dev));

  aesd_circular_buffer_init(&aesd_device.buffer);
  mutex_init(&aesd_device.buffer_mutex);

  result = aesd_setup_cdev(&aesd_device);

  if (result) {
    unregister_chrdev_region(dev, 1);
  }
  return result;
}

void aesd_cleanup_module(void) {
  dev_t devno = MKDEV(aesd_major, aesd_minor);

  cdev_del(&aesd_device.cdev);

  uint8_t index;
  struct aesd_buffer_entry *entry;

  while (mutex_lock_interruptible(&aesd_device.buffer_mutex) != 0) {
  }

  AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
    PDEBUG("Free %.*s", (int)entry->size, entry->buffptr);
    kfree(entry->buffptr);
  }

  mutex_unlock(&aesd_device.buffer_mutex);
  mutex_destroy(&aesd_device.buffer_mutex);
  unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
