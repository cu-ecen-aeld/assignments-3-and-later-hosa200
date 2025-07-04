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
#include "aesd_ioctl.h"
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
    size_t tmp_off;
    ssize_t retval = 0;
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("Error %d", ERESTARTSYS);
        return -ERESTARTSYS;
    }

    aesd_device->read_buf = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device->buffer,
                                                                            *f_pos,
                                                                            &aesd_device->read_off);

    if (aesd_device->read_buf)
    {
        tmp_count = aesd_device->read_buf->size - aesd_device->read_off;
        PDEBUG("tmp_count is %d ,size is %d, read offset is %d", tmp_count, aesd_device->read_buf->size, aesd_device->read_off);
        if (tmp_count >= count)
        {
            tmp_count = count;
            PDEBUG("data numbers are %d as input", tmp_count);
        }
        else
        {
            // tmp_count = aesd_device->read_buf->size;
            PDEBUG("data numbers are %d as buf", tmp_count);
        }

        if (copy_to_user(buf, aesd_device->read_buf->buffptr + aesd_device->read_off, tmp_count))
        {
            PDEBUG("Error writing to userspace");
            retval = -EFAULT;
        }
        else
            PDEBUG("Data is available \n");
        *f_pos += tmp_count;
        retval = tmp_count;
    }
    else
    {
        PDEBUG("Reading is done, returning ");
        retval = 0;
    }

    goto out;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t sum = 0;
    size_t i;
    loff_t new_pos;
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("seeking an offset %ld", off);
    switch (whence)
    {
    case SEEK_SET:
        PDEBUG("seeking, set the offset");
        new_pos = off;
        break;
    case SEEK_CUR:
        PDEBUG("seeking, getting current offset");
        if (mutex_lock_interruptible(&dev->lock))
        {
            PDEBUG("mutex lock failure during llseek");
            return -ERESTARTSYS;
        }
        new_pos = filp->f_pos + off;
        mutex_unlock(&dev->lock);
        break;
    case SEEK_END:
        PDEBUG("seeking, getting the end of the file");
        if (mutex_lock_interruptible(&dev->lock))
        {
            PDEBUG("mutex lock failure during llseek");
            return -ERESTARTSYS;
        }
        for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
        {
            sum += dev->buffer.entry[i].size;
        }
        mutex_unlock(&dev->lock);
        new_pos = off + sum;
        break;
    default:
        return -EINVAL;
    }

    if (new_pos < 0)
        return -EINVAL;

    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("mutex lock failure during llseek");
        return -ERESTARTSYS;
    }
    PDEBUG("seeking, final offset %ld", new_pos);
    filp->f_pos = new_pos;
    mutex_unlock(&dev->lock);

    return new_pos;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    int cntr = 0;
    char ptr_index = 0;
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&dev->lock))
    {
        PDEBUG("Error %d", ERESTARTSYS);
        return -ERESTARTSYS;
    }
    if (aesd_device->buf_page_no > 0)
    {
        ptr_index = aesd_device->local_buf.size;
        aesd_device->local_buf.size += count;                                                                               /* save count of bytes */
        aesd_device->local_buf.buffptr = krealloc(aesd_device->local_buf.buffptr, aesd_device->local_buf.size, GFP_KERNEL); /* allocate memory */
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
    if (copy_from_user(aesd_device->local_buf.buffptr + ptr_index,
                       buf,
                       count)) /* copy to kernal space memory */
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
            PDEBUG("buffer is full, freeing memory");
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

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seekto;
    loff_t off = 0;
    int i = 0;
    struct aesd_dev *dev = filp->private_data;
    long retval = 0;

    PDEBUG("Entering IO ctrl\n");
    PDEBUG("Expecting %u\n", AESDCHAR_IOCSEEKTO);
    PDEBUG("Found %u\n", cmd);
    switch (cmd)
    {
    case AESDCHAR_IOCSEEKTO:
        if (copy_from_user(&seekto,
                           (const void __user *)arg,
                           sizeof(struct aesd_seekto))) /* copy to kernal space memory */
        {
            PDEBUG("Error copying from userspace");
            retval = -EFAULT;
            return retval;
        }
        PDEBUG("pos is %ld, off is %ld", seekto.write_cmd, seekto.write_cmd_offset);
        if (mutex_lock_interruptible(&dev->lock))
        {
            PDEBUG("Error %d", ERESTARTSYS);
            return -ERESTARTSYS;
        }

        if ((seekto.write_cmd < 0) ||
            (seekto.write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) ||
            (seekto.write_cmd_offset < 0) ||
            (seekto.write_cmd_offset > dev->buffer.entry[seekto.write_cmd].size) ||
            (dev->buffer.entry[seekto.write_cmd].buffptr == NULL))
        {
            PDEBUG("Invalid write command or buffer is empty");
            retval = -EINVAL;
            return retval;
        }

        for (i = 0; i < seekto.write_cmd; i++)
        {
            off += dev->buffer.entry[i].size;
        }
        off += seekto.write_cmd_offset;
        filp->f_pos = off;
        retval = off;
        PDEBUG("Command is valid and new file offset is %d", off);
        mutex_unlock(&dev->lock);
        return retval;
        break;

    default:
        PDEBUG("Command is inValid");
        return -ENOTTY;
        break;
    }
}

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
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
