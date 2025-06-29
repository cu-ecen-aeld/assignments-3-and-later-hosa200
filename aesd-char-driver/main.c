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

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Hossam Batekh");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev *aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    /* device info */
    struct aesd_dev *dev;

    PDEBUG("open start");

    /* get cdev location */
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

    /* pass cdev to other methods */
    filp->private_data = dev;

    if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
    {
        if (mutex_lock_interruptible(&dev->lock))
            return -ERESTARTSYS;
        /* TODO: trim ? */
        aesd_device->read_buf = NULL;
        mutex_unlock(&dev->lock);
    }

    PDEBUG("open ok");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("close");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    size_t tmp_count;
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("Error %d", ERESTARTSYS);
        return -ERESTARTSYS;
    }

    if (aesd_device->read_buf == NULL)
    {
        aesd_device->read_buf = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device->buffer,
                                                                                0,
                                                                                &aesd_device->read_off);
        if (aesd_device->read_buf)
        {
            if (count < aesd_device->read_buf->size)
            {
                tmp_count = count;
                PDEBUG("data numbers are %d as input", tmp_count);
            }
            else
            {
                tmp_count = aesd_device->read_buf->size;
                PDEBUG("data numbers are %d as buf", tmp_count);
            }
            if (copy_to_user(buf, aesd_device->read_buf->buffptr, tmp_count))
            {
                PDEBUG("Error writing to userspace");
                retval = -EFAULT;
            }
            else
                PDEBUG("Data is available \n");
            retval = tmp_count;
        }
        else
        {
            PDEBUG("Reading is failing, returning ");
            retval = -EFAULT;
        }
    }
    else
    {
        retval = 0; /* it means no more data */
    }

    goto out;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    int cntr = 0;
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("Error %d", ERESTARTSYS);
        return -ERESTARTSYS;
    }
    if (aesd_device->buf_page_no > 0)
    {
        aesd_device->local_buf.size += count;                                                                        /* save count of bytes */
        aesd_device->local_buf.buffptr = krealloc(&aesd_device->local_buf, aesd_device->local_buf.size, GFP_KERNEL); /* allocate memory */
    }
    else
    {
        aesd_device->local_buf.size = count;                                               /* save count of bytes */
        aesd_device->local_buf.buffptr = kmalloc(aesd_device->local_buf.size, GFP_KERNEL); /* allocate memory */
    }
    if (aesd_device->local_buf.buffptr == NULL)
    {
        PDEBUG("Error allocating memory in kernal space");
        retval = -EFAULT;
        goto out;
    }
    if (copy_from_user(aesd_device->local_buf.buffptr,
                       buf,
                       aesd_device->local_buf.size)) /* copy to kernal space memory */
    {
        PDEBUG("Error copying from userspace");
        retval = -EFAULT;
        goto out;
    }
    retval = count;                                                              /* update return value */
    if (aesd_device->local_buf.buffptr[aesd_device->local_buf.size - 1] == '\n') /* write only if the data ends with a terminator */
    {
        PDEBUG("data with terminator found");

        if (aesd_device->buffer.full)
        {
            kfree(aesd_device->buffer.entry[aesd_device->buffer.in_offs].buffptr);
        }

        if (aesd_device->append_page) /* if a terminator found with many pages */
        {
            PDEBUG("data is appended, multi page");
            PDEBUG("write of size %d of data is done\n", aesd_device->local_buf.size);
            aesd_circular_buffer_add_entry(&aesd_device->buffer, &aesd_device->local_buf);
            aesd_device->buf_page_no = 0; /* writing is done, reset page number */
            aesd_device->append_page = 0;
        }
        else /* single pages */
        {
            PDEBUG("single page");
            PDEBUG("write of size %d of data is done\n", aesd_device->local_buf.size);
            aesd_circular_buffer_add_entry(&aesd_device->buffer, &aesd_device->local_buf);
        }
    }
    else
    {
        aesd_device->buf_page_no++;
        PDEBUG("data without terminator found,now page %d\n", aesd_device->buf_page_no);
        aesd_device->append_page = 1;
    }
    goto out;

out:
    PDEBUG("page no %d\n", aesd_device->buf_page_no);
    mutex_unlock(&dev->lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}
void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    int i;

    cdev_del(&aesd_device->cdev);

    /**
     * cleanup AESD specific poritions here as necessary
     */
    AESD_CIRCULAR_BUFFER_FOREACH(aesd_device->read_buf, &aesd_device->buffer, i)
    {
        kfree(aesd_device->read_buf->buffptr);
    }
    kfree(aesd_device);
    unregister_chrdev_region(devno, 1);
    printk(KERN_INFO "Unloaded module aesdchar");
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    /* allocate the device */
    aesd_device = kmalloc(sizeof(struct aesd_dev), GFP_KERNEL);
    if (!aesd_device)
    {
        result = -ENOMEM;
        goto fail;
    }
    memset(aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * Initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device->lock);
    aesd_circular_buffer_init(&aesd_device->buffer); /* Init our buffer */
    aesd_device->buf_page_no = 0;
    aesd_device->append_page = 0;
    // memset(local_buf, 0, sizeof(local_buf));

    result = aesd_setup_cdev(aesd_device);

    if (result)
    {
        goto fail;
    }
    printk(KERN_INFO "Loaded moudle aesdchar\n");
    return result;

fail:
    aesd_cleanup_module();
    return result;
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
