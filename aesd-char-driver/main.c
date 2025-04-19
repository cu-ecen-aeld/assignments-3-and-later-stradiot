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
#include "aesd_ioctl.h"

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
    /*
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;

	size_t entry_offset;

	if (mutex_lock_interruptible(&dev->lock)){
		return -ERESTARTSYS;
	}

	struct aesd_buffer_entry* entry = aesd_circular_buffer_find_entry_offset_for_fpos(
		&(dev->c_buffer),
		*f_pos,
		&entry_offset
	);

	mutex_unlock(&dev->lock);

	if (entry != NULL){
		size_t unread_bytes = entry->size - entry_offset;
		size_t read_size = (unread_bytes > count) ? count : unread_bytes;

		PDEBUG("Reading message %.*s of size %zu", read_size, entry->buffptr + entry_offset, read_size);
		if (copy_to_user(buf, entry->buffptr + entry_offset, read_size)){
			return -EINTR;
		}
		*f_pos += read_size;
		retval = read_size;
	}

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

	if (mutex_lock_interruptible(&dev->lock)){
		return -ERESTARTSYS;
	}

	dev->c_buffer_entry.buffptr = krealloc(
		dev->c_buffer_entry.buffptr,
		dev->c_buffer_entry.size + count,
		GFP_KERNEL
	);

	if(copy_from_user(
		dev->c_buffer_entry.buffptr + dev->c_buffer_entry.size,
		buf,
		count
	)){
		return -EINTR;
	}
	dev->c_buffer_entry.size += count;

	if(strchr(dev->c_buffer_entry.buffptr, '\n') != NULL){
		const char* deleted_item = aesd_circular_buffer_add_entry(&(dev->c_buffer), &(dev->c_buffer_entry)); 
		PDEBUG("Added entry %s", dev->c_buffer_entry.buffptr);

		if (deleted_item != NULL){
			PDEBUG("Deleted entry %s", deleted_item);
			kfree(deleted_item);
		}

		memset(&dev->c_buffer_entry, 0, sizeof(struct aesd_buffer_entry));
	}

	retval = count;

	*f_pos += count;

	mutex_unlock(&dev->lock);

    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence){
    struct aesd_dev *dev = filp->private_data;

	size_t buff_size = 0;

	uint8_t index;
	struct aesd_buffer_entry *entry;
	AESD_CIRCULAR_BUFFER_FOREACH(entry,&(dev->c_buffer),index) {
		buff_size += entry->size;
	}

	PDEBUG("Seeking offset %ld in buffer with size %ld", offset, buff_size);

	return fixed_size_llseek(filp, offset, whence, buff_size);
}

static long aesd_adjust_file_offset(
	struct file *filp,
	unsigned int write_cmd,
	unsigned int write_cmd_offset
){
	long retval = 0;

    struct aesd_dev *dev = filp->private_data;

	if (write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
		return -EINVAL;
	}

	if (mutex_lock_interruptible(&dev->lock)){
		return -ERESTARTSYS;
	}

	int i, cmd_offset = 0;
	for (i = dev->c_buffer.out_offs; i < write_cmd + dev->c_buffer.out_offs; i++){
		int curr_index = i % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

		cmd_offset += dev->c_buffer.entry[curr_index].size;
	}

	int cmd_index = (write_cmd + dev->c_buffer.out_offs)
		% AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

	if (write_cmd_offset >= dev->c_buffer.entry[cmd_index].size){
		return -EINVAL;
	}

	cmd_offset += write_cmd_offset;
	filp->f_pos = cmd_offset;

	mutex_unlock(&dev->lock);

    return retval;
}

static long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	long retval = 0;

    switch (cmd) {
        case AESDCHAR_IOCSEEKTO: {
			struct aesd_seekto seekto;

			if (
				copy_from_user(
					&seekto,
					(const void  __user *)arg,
					sizeof(seekto)
				) != 0
			) {
				retval = -EFAULT;
			} else {
				retval = aesd_adjust_file_offset(
					filp,
					seekto.write_cmd,
					seekto.write_cmd_offset
				);
			}

			break;
		}
	}

	return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
	.llseek =	aesd_llseek,
    .open =     aesd_open,
    .release =  aesd_release,
	.unlocked_ioctl = aesd_ioctl,
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
