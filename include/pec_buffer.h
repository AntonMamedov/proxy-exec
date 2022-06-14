#include <linux/types.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#define PEC_RCVBUF 4096
#define PEC_SNDBUF 4096

#define EXTEND_FACTOR 4

typedef uint8_t byte_t;

typedef struct pec_buffer {
    byte_t* buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t free_space;
    spinlock_t lock;
} pec_buffer_t;

ssize_t pec_buffer_write(pec_buffer_t* dst, const byte_t* b, size_t size) {
    spin_lock(&dst->lock);

    RESTART:
    if (dst->head == dst->tail) {
        dst->head = dst->tail = 0;
    }

    if (size > dst->free_space) {
        byte_t * buffer = vzalloc(sizeof(dst->size) * EXTEND_FACTOR);
        if (dst->head < dst->tail)
            memcpy(buffer, dst->buffer + dst->head, dst->tail - dst->head);
        else {
            memcpy(buffer, dst->buffer + dst->head, dst->size - dst->head);
            memcpy(buffer + dst->size - dst->head, dst->buffer, dst->tail);
            dst->head = 0;
            dst->tail = dst->size - dst->free_space;
            dst->size *= EXTEND_FACTOR;
            dst->free_space *= dst->size - dst->tail;
            vfree(dst->buffer);
            dst->buffer = buffer;
        }

        if (size > dst->free_space)
            goto RESTART;
    }

    if (size <= dst->size - dst->tail) {
        copy_from_user(dst->buffer + dst->tail, b, size);
    } else {

    }

//    size_t free_space_to_the_end = dst->size - dst->len;
//    size_t free_space = free_space_to_the_end + dst->seek;
//    if (free_space < size) {
//        byte_t * buffer = vzalloc(sizeof(dst->size) * 4);
//        size_t current_step_need_cpy = dst->size - dst->seek;
//        memcpy(buffer, dst->buffer + dst->seek, current_step_need_cpy);
//        current_step_need_cpy = dst->len - current_step_need_cpy;
//        memcpy(buffer + (dst->size - dst->seek), dst->buffer, dst->seek);
//        vfree(dst->buffer);
//        dst->buffer = buffer;
//        dst->size = size * 4;
//
//    }
//
//    int ret = 0;
//    if (free_space_to_the_end < size) {
//        ret = copy_from_user(dst->buffer + dst->seek, b, size);
//        goto OUT;
//    }


    OUT:
    spin_unlock(&dst->lock);
    return ret;
}

ssize_t pec_buffer_read(byte_t *desc, size_t size);

