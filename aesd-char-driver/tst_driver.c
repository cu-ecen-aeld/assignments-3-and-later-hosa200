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

struct aesd_circular_buffer buffer;
char *local_buf[50];
size_t buf_page_size[50];
char buf_page_no;
char append_page;
char *read_buf;
size_t read_off;

size_t aesd_read(/*struct file *filp,*/ char *buf, size_t count ,
                    char f_pos)
{
    size_t retval = 0;
    // struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *return_buf;
    
    PDEBUG("read %zu bytes with offset %d\n", count,f_pos);

    // if (mutex_lock_interruptible(&dev->lock))
    // {
    //     PDEBUG("Error %d", ERESTARTSYS);
    //     return -ERESTARTSYS;
    // }

    return_buf = aesd_circular_buffer_find_entry_offset_for_fpos(&buffer, f_pos, &read_off);
    if (return_buf != NULL)
    {
        // if (copy_to_user(buf, return_buf->buffptr[read_off], count))
        if (!memcpy(buf, return_buf->buffptr+f_pos, count))
        {
            PDEBUG("Error writing %d to userspace",&return_buf->buffptr[read_off]);
            retval = -2;
            goto out;
        }
        else
        {
            // PDEBUG("Data is available %s\n",buf);
        }
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
    struct aesd_buffer_entry add_entry;
    PDEBUG("write %zu bytes with offset 0\n", count);
    // if (mutex_lock_interruptible(&dev->lock))
    // {
    //     PDEBUG("Error %d", ERESTARTSYS);
    //     return -ERESTARTSYS;
    // }

    // local_buf[buf_page_no] = kmalloc(sizeof(char) * count); /* allocate memory */
    local_buf[buf_page_no] = malloc(sizeof(char) * count); /* allocate memory */
    buf_page_size[buf_page_no] = count;
    // if (copy_from_user(local_buf[buf_page_no], buf, count))             /* copy to kernal space memory */
    if (!memcpy(local_buf[buf_page_no], buf, buf_page_size[buf_page_no])) /* copy to kernal space memory */
    {
        PDEBUG("Error copying from userspace\n");
        retval = -2;
        goto out;
    }

    if (local_buf[buf_page_no][buf_page_size[buf_page_no] - 1] == '\n') /* write only if the data ends with a terminator */
    {
        PDEBUG("data with terminator found\n");
        if (append_page) /* if a terminator found with many pages */
        {
            PDEBUG("data is appended\n");
            append_page = 0;
            for (cntr = 0; cntr <= buf_page_no ;cntr++) /* loop on all pages, write them and free local buffer */
            {
                add_entry.buffptr = local_buf[cntr];
                add_entry.size = buf_page_size[cntr];
                PDEBUG("write single page data %c size of %d\n", *add_entry.buffptr, add_entry.size);
                aesd_circular_buffer_add_entry(&buffer, &add_entry);
                // kfree(local_buf[buf_page_no]);
                // free(local_buf[buf_page_no]);
                PDEBUG("write single page data done %c\n", *buffer.entry[0].buffptr);
            }
            buf_page_no = 0; /* writing is done, reset page number */
        }
        else
        {
            // PDEBUG("write single page data\n");
            add_entry.buffptr = local_buf[buf_page_no];
            add_entry.size = buf_page_size[cntr];
            PDEBUG("write single page data %c size of %d\n", *add_entry.buffptr, add_entry.size);
            aesd_circular_buffer_add_entry(&buffer, &add_entry);
            // kfree(local_buf[buf_page_no]);
            // free(local_buf[buf_page_no]);
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
    
    aesd_circular_buffer_init(&buffer);
    // write_circular_buffer_packet(&buffer, "write1\n");
    // write_circular_buffer_packet(&buffer, "write2\n");
    // write_circular_buffer_packet(&buffer, "write3\n");
    // write_circular_buffer_packet(&buffer, "write4\n");
    // write_circular_buffer_packet(&buffer, "write5\n");
    // write_circular_buffer_packet(&buffer, "write6\n");
    // write_circular_buffer_packet(&buffer, "write7\n");
    // write_circular_buffer_packet(&buffer, "write8\n");
    // write_circular_buffer_packet(&buffer, "write9\n");
    // write_circular_buffer_packet(&buffer, "write10\n");
    write_circular_buffer_packet(&buffer, "write9");
    write_circular_buffer_packet(&buffer, "write10\n");

    aesd_read(tmp_read_buf,7,1);
    printf("%s\n",tmp_read_buf);
    // verify_find_entry(&buffer, 0, "write1\n");
    // verify_find_entry(&buffer, 7, "write2\n");
    // verify_find_entry(&buffer, 14, "write3\n");
    // verify_find_entry(&buffer, 21, "write4\n");
    // verify_find_entry(&buffer, 28, "write5\n");
    // verify_find_entry(&buffer, 35, "write6\n");
    // verify_find_entry(&buffer, 42, "write7\n");
    // verify_find_entry(&buffer, 49, "write8\n");
    // verify_find_entry(&buffer, 0, "write9write10\n");
    // verify_find_entry(&buffer, 63, "write10\n");

    // verify_find_entry(&buffer, 70, "\n");

    // verify_find_entry_not_found(&buffer, 71);

    // write_circular_buffer_packet(&buffer, "write11\n");

    // verify_find_entry(&buffer, 0, "write2\n");
    // verify_find_entry(&buffer, 7, "write3\n");
    // verify_find_entry(&buffer, 14, "write4\n");
    // verify_find_entry(&buffer, 21, "write5\n");
    // verify_find_entry(&buffer, 28, "write6\n");
    // verify_find_entry(&buffer, 35, "write7\n");
    // verify_find_entry(&buffer, 42, "write8\n");
    // verify_find_entry(&buffer, 49, "write9\n");
    // verify_find_entry(&buffer, 56, "write10\n");
    // verify_find_entry(&buffer, 64, "write11\n");
    // verify_find_entry(&buffer, 71, "\n");
}