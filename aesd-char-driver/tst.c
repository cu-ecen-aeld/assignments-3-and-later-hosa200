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
#endif

#include "aesd-circular-buffer.h"
#include "stdio.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
                                                                          size_t char_offset, size_t *entry_offset_byte_rtn)
{
    /**
     * TODO: implement per description
     */
    uint8_t index_out = 0; // buffer->out_offs;
    size_t size_offset = char_offset;
    size_t size_looper = 0;
    uint8_t max_index = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (buffer->full == 1)
    {
        index_out = buffer->out_offs;
    }

    for (; index_out < max_index; index_out++)
    {
        if (size_offset < buffer->entry[index_out].size)
        {
            *entry_offset_byte_rtn = size_offset;
            return &buffer->entry[index_out];
        }
        else
        {

            size_offset = char_offset - buffer->entry[index_out].size - size_looper;
            size_looper += buffer->entry[index_out].size;
            // if (size_offset != 0)
            //     size_offset--;
        }
    }
    if ( (buffer->full == 1)  && (buffer->out_offs != 0 ))
    {
        index_out = 0;
        size_offset = char_offset - size_looper;
        ;
        if (size_offset < buffer->entry[index_out].size)
        {
            *entry_offset_byte_rtn = size_offset;
            return &buffer->entry[index_out];
        }
    }
    return NULL;
}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
 * If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
 * new start location.
 * Any necessary locking must be handled by the caller
 * Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
 */
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
     * TODO: implement per description
     */
    buffer->entry[buffer->in_offs] = *add_entry;

    if (buffer->in_offs == (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1))
    {
        buffer->in_offs = 0;
        buffer->full = 1;
    }
    else
    {
        buffer->in_offs++;
        if (buffer->full == 1)
        {
            buffer->out_offs = 1;
        }
    }
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}

static void write_circular_buffer_packet(struct aesd_circular_buffer *buffer,
                                         const char *writestr)
{
    struct aesd_buffer_entry entry;
    entry.buffptr = writestr;
    entry.size = strlen(writestr);
    aesd_circular_buffer_add_entry(buffer, &entry);
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
    snprintf(message, sizeof(message), "entry string does not match expected value at offset %zu",
             entry_offset_byte);
    // TEST_ASSERT_EQUAL_STRING_MESSAGE(expectstring,&rtnentry->buffptr[offset_rtn],message);
    printf("%s      %s\n", expectstring, &rtnentry->buffptr[offset_rtn]);
    printf("%s\n", message);

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
    struct aesd_circular_buffer buffer;
    aesd_circular_buffer_init(&buffer);
    write_circular_buffer_packet(&buffer, "write1\n");
    write_circular_buffer_packet(&buffer, "write2\n");
    write_circular_buffer_packet(&buffer, "write3\n");
    write_circular_buffer_packet(&buffer, "write4\n");
    write_circular_buffer_packet(&buffer, "write5\n");
    write_circular_buffer_packet(&buffer, "write6\n");
    write_circular_buffer_packet(&buffer, "write7\n");
    write_circular_buffer_packet(&buffer, "write8\n");
    write_circular_buffer_packet(&buffer, "write9\n");
    write_circular_buffer_packet(&buffer, "write10\n");

    verify_find_entry(&buffer, 0, "write1\n");
    verify_find_entry(&buffer, 7, "write2\n");
    verify_find_entry(&buffer, 14, "write3\n");
    verify_find_entry(&buffer, 21, "write4\n");
    verify_find_entry(&buffer, 28, "write5\n");
    verify_find_entry(&buffer, 35, "write6\n");
    verify_find_entry(&buffer, 42, "write7\n");
    verify_find_entry(&buffer, 49, "write8\n");
    verify_find_entry(&buffer, 56, "write9\n");
    verify_find_entry(&buffer, 63, "write10\n");

    verify_find_entry(&buffer, 70, "\n");

    verify_find_entry_not_found(&buffer, 71);

    write_circular_buffer_packet(&buffer, "write11\n");

    verify_find_entry(&buffer, 0, "write2\n");
    verify_find_entry(&buffer, 7, "write3\n");
    verify_find_entry(&buffer, 14, "write4\n");
    verify_find_entry(&buffer, 21, "write5\n");
    verify_find_entry(&buffer, 28, "write6\n");
    verify_find_entry(&buffer, 35, "write7\n");
    verify_find_entry(&buffer, 42, "write8\n");
    verify_find_entry(&buffer, 49, "write9\n");
    verify_find_entry(&buffer, 56, "write10\n");
    verify_find_entry(&buffer, 64, "write11\n");
    verify_find_entry(&buffer, 71, "\n");
    // verify_find_entry_not_found(&buffer,72);
}