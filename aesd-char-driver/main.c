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
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
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
	
	bytes_to_copy = min(count, entry->size - offset);
	
	if (copy_to_user(buf, buf_entry->buffptr + offset, bytes_to_copy)) 
    {
        retval = -EFAULT;
        goto unlock_mutex;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;
	
	unlock_mutex:
		mutex_unlock(&devp->mutex_lock);
	exit:	
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
	
	temp = kmalloc(count, GFP_KERNEL);
    if (!temp)
    {
        retval = -ENOMEM;
        goto exit;
    }
    
    if (copy_from_user(temp, buf, count))
    {
        retval = -EFAULT;
        kfree(temp);
        goto exit;
    }
    
    char* end_of_text;
    size_t write_bytes = count;
    end_of_text = memchr(temp, '\n', count);
    if(end_of_text != NULL)
    {
        write_bytes = (end_of_text - temp) + 1;
    }

    devp = filp->private_data;
    
	if (mutex_lock_interruptible(&devp->lock)) 
    {
        retval = -ERESTARTSYS;
        kfree(temp);
        goto unlock_mutex;
    }
    
    devp->buffer_entry.buffptr = krealloc(devp->buffer_entry.buffptr, 
    								devp->buffer_entry.size + write_bytes, GFP_KERNEL);
    if(devp->buffer_entry.buffptr == NULL)
    {
        retval = -ENOMEM;
        kfree(temp);
        goto unlock_mutex;
    }
    
    memcpy(devp->entry.buffptr + devp->entry.size, temp, write_bytes);

    devp->entry.size += write_bytes;
    
    if (end_of_text)
    {
        const char *bufp;
        bufp = NULL;
        
        bufp = aesd_circular_buffer_add_entry(&devp->buf, &devp->entry);
        if (bufp)
        {
            kfree(temp);
        }

        dev->entry.size = 0;
        dev->entry.buffptr = NULL;
    }
	
    unlock_mutex:
		mutex_unlock(&devp->lock);
	exit:	
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
