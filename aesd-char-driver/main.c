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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Martin Stradiot");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
	filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev); 
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;

	size_t unused;

	struct aesd_buffer_entry* entry = aesd_circular_buffer_find_entry_offset_for_fpos(
		&(dev->c_buffer),
		*f_pos,
		&unused
	);

	if (entry != NULL){
		PDEBUG("found message %s, size %zu, f_pos %lld", entry->buffptr, entry->size, *f_pos);
		if (copy_to_user(buf, entry->buffptr, entry->size)){
			return -EINTR;
		}
		*f_pos += entry->size;
		retval = entry->size;
	}

	PDEBUG("end retval %zu", retval);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("init vals buffer %s size %zu",dev->c_buffer_entry.buffptr,dev->c_buffer_entry.size,*f_pos);

	if (mutex_lock_interruptible(&dev->lock)){
		return -ERESTARTSYS;
	}

	dev->c_buffer_entry.buffptr = krealloc(
		dev->c_buffer_entry.buffptr,
		dev->c_buffer_entry.size + count,
		GFP_KERNEL
	);

	copy_from_user(
		dev->c_buffer_entry.buffptr + dev->c_buffer_entry.size,
		buf,
		count
	);
	dev->c_buffer_entry.size += count;

    PDEBUG("ACT buffer %s", dev->c_buffer_entry.buffptr);

	if(strchr(dev->c_buffer_entry.buffptr, '\n') != NULL){
		const char* deleted_item = aesd_circular_buffer_add_entry(&(dev->c_buffer), &(dev->c_buffer_entry)); 

		if (deleted_item != NULL){
			PDEBUG("Deleted entry %s", deleted_item);
			kfree(deleted_item);
		}	

		memset(&dev->c_buffer_entry, 0, sizeof(struct aesd_buffer_entry));
		PDEBUG("entry added");
	}

	retval = count;

	mutex_unlock(&dev->lock);

    PDEBUG("returning %zu", retval);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.c_buffer);
	mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
