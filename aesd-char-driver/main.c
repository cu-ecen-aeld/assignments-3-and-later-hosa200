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
#include "aesd-circular-buffer.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Hossam Batekh");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev *aesd_device;
struct aesd_circular_buffer buffer;
char *local_buf[50];
size_t buf_page_size[50];
char buf_page_no;
char append_page;
char *read_buf;
size_t read_off;

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
        aesd_circular_buffer_init(&buffer); /* Init our buffer */
        buf_page_no = 0;
        append_page = 0;
        memset(local_buf, 0, 50 * sizeof(char));
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
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *return_buf;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("Error %d", ERESTARTSYS);
        return -ERESTARTSYS;
    }

    return_buf = aesd_circular_buffer_find_entry_offset_for_fpos(&buffer, (size_t)*f_pos, &read_off);
    if (return_buf != NULL)
    {
        if (copy_to_user(buf, return_buf->buffptr+(char)*f_pos, count))
        {
            PDEBUG("Error writing %p to userspace", &return_buf->buffptr[read_off]);
            retval = -EFAULT;
            goto out;
        }
        else
            PDEBUG("Data is available %s\n",buf);
    }
    else
    {
        PDEBUG("Reading is failing, returning ");
        goto out;
    }
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
    struct aesd_buffer_entry add_entry;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("Error %d", ERESTARTSYS);
        return -ERESTARTSYS;
    }

    local_buf[buf_page_no] = kmalloc(sizeof(char) * count, GFP_KERNEL);          /* allocate memory */
    if (copy_from_user(local_buf[buf_page_no], buf, buf_page_size[buf_page_no])) /* copy to kernal space memory */
    {
        PDEBUG("Error copying from userspace");
        retval = -EFAULT;
        goto out;
    }
    if (local_buf[buf_page_no][buf_page_size[buf_page_no] - 1] == '\n') /* write only if the data ends with a terminator */
    {
        PDEBUG("data with terminator found");
        if (append_page) /* if a terminator found with many pages */
        {
            PDEBUG("data is appended");
            append_page = 0;
            for (cntr = 0; cntr <= buf_page_no; cntr++) /* loop on all pages, write them and free local buffer */
            {
                add_entry.buffptr = local_buf[cntr];
                add_entry.size = buf_page_size[cntr];
                PDEBUG("write single page data %c size of %d\n", *add_entry.buffptr, add_entry.size);
                aesd_circular_buffer_add_entry(&buffer, &add_entry);
                // kfree(local_buf[buf_page_no]);
                PDEBUG("write single page data done %c\n", *buffer.entry[0].buffptr);
            }
            buf_page_no = 0; /* writing is done, reset page number */
        }
        else
        {
            add_entry.buffptr = local_buf[buf_page_no];
            add_entry.size = buf_page_size[buf_page_no];
            PDEBUG("write single page data %c size of %d\n", *add_entry.buffptr, add_entry.size);
            aesd_circular_buffer_add_entry(&buffer, &add_entry);
            // kfree(local_buf[buf_page_no]);
            PDEBUG("write single page data done %c\n", *buffer.entry[0].buffptr);
        }
    }
    else
    {
        buf_page_no++;
        PDEBUG("data without terminator found,now page %d\n", buf_page_no);
        append_page = 1;
    }
    goto out;

out:
    PDEBUG("page no %d\n", buf_page_no);
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

    cdev_del(&aesd_device->cdev);

    /**
     * cleanup AESD specific poritions here as necessary
     */
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
    // aesd_circular_buffer_init(&aesd_device.buffer);

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
