#include "aesd-circular-buffer.h"
#include <string.h>

/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#include <stdlib.h>
#endif

#include "stdio.h"
#include "aesdchar.h"
#include "aesd-circular-buffer.h"

struct aesd_dev *aesd_device;

size_t aesd_read(/*struct file *filp,*/ char *buf, size_t count,
                 char f_pos)
{
    size_t retval = 0;
    // struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *return_buf;

    PDEBUG("read %zu bytes with offset %d\n", count, f_pos);

    // if (mutex_lock_interruptible(&dev->lock))
    // {
    //     PDEBUG("Error %d", ERESTARTSYS);
    //     return -ERESTARTSYS;
    // }

    aesd_device->read_buf = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device->buffer,
                                                                            (size_t)f_pos,
                                                                            &aesd_device->read_off);
    if (aesd_device->read_buf)
    {
        // if (copy_to_user(buf, return_buf->buffptr[read_off], count))
        if (!memcpy(buf, aesd_device->read_buf->buffptr, count))
        {
            PDEBUG("Error writing to userspace");
            retval = -2;
            goto out;
        }
        else
        {
            PDEBUG("Data is available \n");
        }
        retval = count;
    }
    else
    {
        PDEBUG("Reading is failing, returning ");
        goto out;
    }
out:
    // mutex_unlock(&dev->lock);
    return retval;
}

long int aesd_write(/*struct file *filp,*/ const char *buf, size_t count /*,*/
                    /*char *f_pos*/)
{
    long int retval = -1;
    int cntr = 0;
    // struct aesd_dev *dev = filp->private_data;
    PDEBUG("write %zu bytes with offset 0\n", count);
    // if (mutex_lock_interruptible(&dev->lock))
    // {
    //     PDEBUG("Error %d", ERESTARTSYS);
    //     return -ERESTARTSYS;
    // }

    if (aesd_device->buf_page_no > 0)
    {
        aesd_device->local_buf.size += count;                                                           /* save count of bytes */
        aesd_device->local_buf.buffptr = realloc(&aesd_device->local_buf, aesd_device->local_buf.size); /* allocate memory */
    }
    else
    {
        aesd_device->local_buf.size = count;                                  /* save count of bytes */
        aesd_device->local_buf.buffptr = malloc(aesd_device->local_buf.size); /* allocate memory */
    }
    if (aesd_device->local_buf.buffptr == NULL)
    {
        PDEBUG("Error allocating memory in kernal space");
        retval = -1;
        goto out;
    }
    /* copy to kernal space memory */
    if (!memcpy(aesd_device->local_buf.buffptr,
                buf,
                aesd_device->local_buf.size)) /* copy to kernal space memory */
    {
        PDEBUG("Error copying from userspace\n");
        retval = -2;
        goto out;
    }
    retval = count;
    if (aesd_device->local_buf.buffptr[aesd_device->local_buf.size - 1] == '\n') /* write only if the data ends with a terminator */
    {
        PDEBUG("data with terminator found\n");
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
    // mutex_unlock(&dev->lock);
    return retval;
}

static void write_circular_buffer_packet(struct aesd_circular_buffer *buffer,
                                         const char *writestr)
{
    struct aesd_buffer_entry entry;
    int x = 0;
    entry.buffptr = writestr;
    entry.size = strlen(writestr);
    x = entry.size;
    aesd_write(writestr, x);
    // aesd_circular_buffer_add_entry(buffer, &entry);
}

static void verify_find_entry(struct aesd_circular_buffer *buffer, size_t entry_offset_byte, const char *expectstring)
{
    size_t offset_rtn = 0;
    char message[250];
    struct aesd_buffer_entry *rtnentry = aesd_circular_buffer_find_entry_offset_for_fpos(buffer,
                                                                                         entry_offset_byte,
                                                                                         &offset_rtn);

    // snprintf(message,sizeof(message),"null pointer unexpected when verifying offset %zu with expect string %s",
    // entry_offset_byte, expectstring);
    // TEST_ASSERT_NOT_NULL_MESSAGE(rtnentry,message);
    // snprintf(message, sizeof(message), "entry string does not match expected value at offset %zu",
    //          entry_offset_byte);
    // TEST_ASSERT_EQUAL_STRING_MESSAGE(expectstring,&rtnentry->buffptr[offset_rtn],message);
    printf("%s      %s\n", expectstring, &rtnentry->buffptr[offset_rtn]);
    // printf("%s\n", message);

    // TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)rtnentry->size,(uint32_t)strlen(rtnentry->buffptr),
    // "size parameter in buffer entry should match total entry length");
}

static void verify_find_entry_not_found(struct aesd_circular_buffer *buffer, size_t entry_offset_byte)
{
    size_t offset_rtn;
    char message[150];
    struct aesd_buffer_entry *rtnentry = aesd_circular_buffer_find_entry_offset_for_fpos(buffer,
                                                                                         entry_offset_byte,
                                                                                         &offset_rtn);
    snprintf(message, sizeof(message), "Expected null pointer when trying to validate entry offset %zu", entry_offset_byte);
    // TEST_ASSERT_NULL_MESSAGE(rtnentry,message);
    if (rtnentry != NULL)
        printf("%s", message);
    else
        printf("It's a null pointer");
}

void main()
{
    // struct aesd_circular_buffer buffer;
    char tmp_read_buf[40];
    aesd_device = malloc(sizeof(struct aesd_dev));
    memset(aesd_device, 0, sizeof(struct aesd_dev));
    aesd_circular_buffer_init(&aesd_device->buffer);
    // write_circular_buffer_packet(&aesd_device->buffer, "write1\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write2\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write3\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write4\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write5\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write6\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write7\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write8\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write9\n");
    // write_circular_buffer_packet(&aesd_device->buffer, "write10\n");
    write_circular_buffer_packet(&aesd_device->buffer, "write9\n");
    write_circular_buffer_packet(&aesd_device->buffer, "write10\n");

    aesd_read(tmp_read_buf, 14, 0);
    printf("res: %s\n", tmp_read_buf);
    // verify_find_entry(&aesd_device->buffer, 0, "write1\n");
    // verify_find_entry(&aesd_device->buffer, 7, "write2\n");
    // verify_find_entry(&aesd_device->buffer, 14, "write3\n");
    // verify_find_entry(&aesd_device->buffer, 21, "write4\n");
    // verify_find_entry(&aesd_device->buffer, 28, "write5\n");
    // verify_find_entry(&aesd_device->buffer, 35, "write6\n");
    // verify_find_entry(&aesd_device->buffer, 42, "write7\n");
    // verify_find_entry(&aesd_device->buffer, 49, "write8\n");
    // verify_find_entry(&aesd_device->buffer, 56, "write9\n");
    // verify_find_entry(&aesd_device->buffer, 63, "write10\n");

    // verify_find_entry(&aesd_device->buffer, 70, "\n");

    // verify_find_entry_not_found(&aesd_device->buffer, 71);

    // write_circular_buffer_packet(&aesd_device->buffer, "write11\n");

    // verify_find_entry(&aesd_device->buffer, 0, "write2\n");
    // verify_find_entry(&aesd_device->buffer, 7, "write3\n");
    // verify_find_entry(&aesd_device->buffer, 14, "write4\n");
    // verify_find_entry(&aesd_device->buffer, 21, "write5\n");
    // verify_find_entry(&aesd_device->buffer, 28, "write6\n");
    // verify_find_entry(&aesd_device->buffer, 35, "write7\n");
    // verify_find_entry(&aesd_device->buffer, 42, "write8\n");
    // verify_find_entry(&aesd_device->buffer, 49, "write9\n");
    // verify_find_entry(&aesd_device->buffer, 56, "write10\n");
    // verify_find_entry(&aesd_device->buffer, 64, "write11\n");
    // verify_find_entry(&aesd_device->buffer, 71, "\n");
}