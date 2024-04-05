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
 * Modified by suhas
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include <linux/slab.h>
#include "aesd_ioctl.h"

#define	SEEK_SET (0)
#define	SEEK_CUR (1)
#define	SEEK_END (2)

/*
typedef enum {
	SEEK_SET = 0,
	SEEK_CUR,
	SEEK_END
}seek_op;
*/

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Suhas Reddy S"); /** TODO: fill in your name **/
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
    filp->private_data = NULL;
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
	     
	// validate inputs
	if(filp==NULL || buf==NULL) {
		retval = -EINVAL;
		goto exit;
	}
	
	struct aesd_dev *devp;
	devp = filp->private_data;
	
	if(!devp) {               
		retval = -EPERM;
		goto exit;
	}
	
	if (mutex_lock_interruptible(&devp->lock)) 
    {
        retval = -ERESTARTSYS;
        goto exit;
    }
	
	struct aesd_buffer_entry *buf_entry;
	size_t offset;
	buf_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&devp->buf, *f_pos, &offset);
    if (!buf_entry) 
    {
        retval = 0;
        goto unlock_mutex;
    }
	
	size_t bytes_to_copy;
	bytes_to_copy = min(count, buf_entry->size - offset);
	
	if (copy_to_user(buf, buf_entry->buffptr + offset, bytes_to_copy)) 
    {
        retval = -EFAULT;
        goto unlock_mutex;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;
	
	unlock_mutex:
		mutex_unlock(&devp->lock);
	exit:	
    	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    // Validate inputs
    if (!filp || !buf) {
        retval = -EINVAL;
        goto exit;
    }

    // Get a reference to the device structure
    struct aesd_dev *devp = filp->private_data;
    if (!devp) {
        retval = -EPERM;
        goto exit;
    }

    // Allocate temporary buffer for user data
    char *temp = kmalloc(count, GFP_KERNEL);
    if (!temp) {
        retval = -ENOMEM;
        goto exit;
    }

    // Copy data from user space
    if (copy_from_user(temp, buf, count)) {
        retval = -EFAULT;
        kfree(temp);
        goto exit;
    }

    // Find end of text ('\n')
    char *end_of_text = memchr(temp, '\n', count);
    size_t write_bytes = count;
    if (end_of_text) {
        write_bytes = end_of_text - temp + 1;
    }

    // Lock mutex to protect shared data
    if (mutex_lock_interruptible(&devp->lock)) {
        retval = -ERESTARTSYS;
        kfree(temp);
        goto exit;
    }	
	
    // Reallocate memory for the entry buffer
    devp->entry.buffptr = krealloc(devp->entry.buffptr, devp->entry.size + write_bytes, GFP_KERNEL);
    if (!devp->entry.buffptr) {
        retval = -ENOMEM;
        mutex_unlock(&devp->lock);
        kfree(temp);
        goto exit;
    }

    // Copy data to entry buffer
    memcpy(devp->entry.buffptr + devp->entry.size, temp, write_bytes);
    kfree(temp);
    devp->entry.size += write_bytes;

    // Add entry to circular buffer if end of text is found
    if (end_of_text) {
        const char *bufp = aesd_circular_buffer_add_entry(&devp->buf, &devp->entry);
        if (bufp) {
		//if (devp->buf.full) {
		    kfree(bufp); // Free memory associated with overwritten entry if circular buffer is full
			//}
	    }
		
        devp->entry.size = 0;
        devp->entry.buffptr = NULL;
    }

    mutex_unlock(&devp->lock);
    retval = write_bytes;

exit:
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int operation) {
	
	struct aesd_dev *devp = filp->private_data;
	size_t size = 0, i;
	loff_t f_pos;
	
	switch(operation) {
		case SEEK_SET: 
			f_pos = off;
			break;
		case SEEK_CUR:
			f_pos = filp->f_pos + off;
			break;
		case SEEK_END:
			if (mutex_lock_interruptible(&devp->lock)) {
				return -ERESTARTSYS;
			}
			for(i = devp->buf.out_offs; i != devp->buf.in_offs; i++) {
				size += devp->buf.entry[i].size;
				i %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
			}
			mutex_unlock(&devp->lock);
			f_pos = size + off;
			break;
		default:
			return -EINVAL;
	}
	filp->f_pos = f_pos;
	return f_pos;
	
}

long int aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	struct aesd_dev *devp = filp->private_data;
	struct aesd_seekto seekto;
	size_t index, i, size = 0;
	
	switch (cmd) {
		case AESDCHAR_IOCSEEKTO:
			if(copy_from_user(&seekto, (struct aesd_seekto *)arg, sizeof(struct aesd_seekto))) {
				return -EFAULT;
			}			
			if(seekto.write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
				return -EINVAL;
			}
			if (mutex_lock_interruptible(&devp->lock)) {
				return -ERESTARTSYS;
			}
			index = devp->buf.out_offs + seekto.write_cmd;
			index %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
			if(seekto.write_cmd_offset > devp->buf.entry[index].size) {
				mutex_unlock(&devp->lock);
				return -EINVAL;
			}
			for(i = devp->buf.out_offs; i != index; i++) {
				size += devp->buf.entry[i].size;
				i %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
			}
			filp->f_pos = size + seekto.write_cmd_offset;
			mutex_unlock(&devp->lock);
			return 0;
		default:
			return -EINVAL;
			
	}
}

struct file_operations aesd_fops = {
    .owner          = THIS_MODULE,
    .read           = aesd_read,
    .write          = aesd_write,
    .open           = aesd_open,
    .release        = aesd_release,
    .llseek         = aesd_llseek,
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

	mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buf);	
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
	
	uint8_t idx = 0;
	struct aesd_buffer_entry *buf_entry;
	AESD_CIRCULAR_BUFFER_FOREACH(buf_entry, &aesd_device.buf, idx) 
    {
        kfree(buf_entry->buffptr);
    }
    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
