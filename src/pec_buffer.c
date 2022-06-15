#include "pec_buffer.h"

#include <linux/kernel.h>
void pec_ring_buffer_init(struct pec_ring_buffer* dst, size_t size)
{
    dst->head = vmalloc(size);
    dst->payload_head = dst->payload_tail = dst->head;
    dst->tail = dst->head + size;
    dst->payload_len = 0;
    dst->size = size;
}

int pec_ring_buffer_read(struct pec_ring_buffer* src, u8* dst, u32 len)
{
    if(src->payload_len == 0) return 0;

    if(len > src->payload_len) len = src->payload_len;

    if(src->payload_head + len > src->tail)
    {
        int len1 = src->tail - src->payload_head;
        int len2 = len - len1;
        copy_to_user(dst, src->payload_head, len1);
        copy_to_user(dst + len1, src->head, len2);
        src->payload_head = src->head + len2;
    }else
    {
        copy_to_user(dst, src->payload_head, len);
        src->payload_head = src->payload_head + len;
    }
    src->payload_len -= len;

    return len;
}

static int pec_ring_buffer_read_internal(struct pec_ring_buffer* src, u8* dst, u32 len)
{
    if(src->payload_len == 0) return 0;

    if(len > src->payload_len) len = src->payload_len;

    if(src->payload_head + len > src->tail)
    {
        int len1 = src->tail - src->payload_head;
        int len2 = len - len1;
        memcpy(dst, src->payload_head, len1);
        memcpy(dst + len1, src->head, len2);
        src->payload_head = src->head + len2;
    }else
    {
        memcpy(dst, src->payload_head, len);
        src->payload_head = src->payload_head + len;
    }
    src->payload_len -= len;

    return len;
}

int pec_ring_buffer_write(struct pec_ring_buffer* dst, const u8* src, u32 src_str_len)
{
    if(src_str_len > dst->size - dst->payload_len) {
        struct pec_ring_buffer new_buffer;
        pec_ring_buffer_init(&new_buffer, dst->size * 2);
        new_buffer.payload_len = dst->payload_len;
        pec_ring_buffer_read_internal(dst, new_buffer.head, dst->payload_len);
        new_buffer.payload_tail += new_buffer.payload_len;
        vfree(dst->head);
        *dst = new_buffer;
    }

    if(dst->payload_tail + src_str_len > dst->tail)
    {
        int len1 = dst->tail - dst->payload_tail;
        int len2 = src_str_len - len1;
        copy_from_user(dst->payload_tail, src, len1);
        copy_from_user(dst->head, src + len1, len2);
        dst->payload_tail = dst->head + len2;
    }else
    {
        copy_from_user(dst->payload_tail, src, src_str_len);
        dst->payload_tail += src_str_len;
    }

    if(dst->payload_len + src_str_len > dst->size)
    {
        int move_len = dst->payload_len + src_str_len - dst->size;
        if(dst->payload_head + move_len > dst->tail)
        {
            int len1 = dst->tail - dst->payload_head;
            int len2 = move_len - len1;
            dst->payload_head = dst->head + len2;
        } else
            dst->payload_head = dst->payload_head + move_len;

        dst->payload_len = dst->size;
    }else
    {
        dst->payload_len += src_str_len;
    }

    return 0;
}

void pec_ring_buffer_destroy(struct pec_ring_buffer* dst) {
    vfree(dst->head);
}