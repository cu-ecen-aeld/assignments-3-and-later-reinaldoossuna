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

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

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

  PDEBUG("Read: %s", entry->buffptr);
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
  /**
   * TODO: handle partial write
   */

  struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
  if (mutex_lock_interruptible(&dev->buffer_mutex)) {
    return -EINTR;
  }

  char *buffptr = (char *)kmalloc(count, GFP_KERNEL);
  if (!buffptr) {
    goto out;
  }

  if (copy_from_user(buffptr, buf, count)) {
    retval = -EFAULT;
    kfree(buffptr);
    goto out;
  }

  struct aesd_buffer_entry entry = {.buffptr = buffptr, .size = count};
  PDEBUG("Wrote: %s", buffptr);
  struct aesd_buffer_entry old_entry = aesd_circular_buffer_add_entry(&dev->buffer, &entry);
  if(old_entry.buffptr) {
    PDEBUG("Free overwritten: %s", old_entry.buffptr);
    kfree(old_entry.buffptr);
  }


out:
  mutex_unlock(&dev->buffer_mutex);
  return retval;
}
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
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
    PDEBUG("Free %s", entry->buffptr);
    kfree(entry->buffptr);
  }

  mutex_unlock(&aesd_device.buffer_mutex);
  mutex_destroy(&aesd_device.buffer_mutex);
  unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
