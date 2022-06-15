//#include "stdlib.h"
//#include "stdint.h"
//#include "string.h"
//typedef  uint8_t byte_t;
//#define EXTEND_FACTOR 2
//typedef struct pec_buffer {
//    byte_t* buffer;
//    size_t size;
//    byte_t* head;
//    byte_t* tail;
//} pec_buffer_t;
//
//size_t pec_buffer_free_space(pec_buffer_t* src) {
//
//}
//
//
////ssize_t pec_buffer_write(pec_buffer_t* dst, const byte_t* b, size_t size) {
////    RESTART:
////    if (dst->head == dst->tail) {
////        dst->head = dst->tail = 0;
////    }
////
////    if (size > dst->free_space) {
////        byte_t * buffer = calloc(dst->size * EXTEND_FACTOR, 1);
////        if (dst->head < dst->tail) {
////            memcpy(buffer, dst->buffer + dst->head, dst->tail - dst->head);
////        }
////        else {
////            memcpy(buffer, dst->buffer + dst->head, dst->size - dst->head);
////            memcpy(buffer + dst->size - dst->head, dst->buffer, dst->tail);
////            dst->head = 0;
////            dst->tail = dst->size - dst->free_space;
////            free(dst->buffer);
////            dst->buffer = buffer;
////        }
////        dst->size *= EXTEND_FACTOR;
////        dst->free_space = dst->size - dst->tail;
////
////        if (size > dst->free_space)
////            goto RESTART;
////    }
////
////    if (size <= dst->size - dst->tail) {
////        strncpy(dst->buffer + dst->tail, b, size);
////        dst->tail += size;
////    } else {
////        strncpy(dst->buffer + dst->tail, b, dst->size - dst->tail);
////        strncpy(dst->buffer, b + (size - dst->tail), size - (dst->size - dst->tail));
////        dst->tail = size - (dst->size - dst->tail);
////    }
////
////    dst->free_space -= size;
////
////    return size;
////}
////
////ssize_t pec_buffer_read(pec_buffer_t* src, byte_t *dst, size_t size) {
////    size_t payload_len = src->size - src->free_space;
////    if (src->size - src->head < size) {
////        strncpy(dst, src->buffer + dst->)
////    }
////}
//
//#define DEFAULT_SIZE 10
//
//int main() {
//    pec_buffer_t buffer = {
//            .tail = 0,
//            .head = 0,
//            .free_space = DEFAULT_SIZE,
//            .size = DEFAULT_SIZE,
//            .buffer = calloc(DEFAULT_SIZE, 1)
//    };
//
//    const byte_t * test = "sadsadsadasdadsasdasdasdasdsads";
//    pec_buffer_write(&buffer, test, strlen(test));
//    pec_buffer_write(&buffer, test, strlen(test));
//    buffer.head += 21;
//    buffer.free_space += 21;
//    pec_buffer_write(&buffer, test, strlen(test));
//}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


typedef unsigned char u8;
typedef unsigned int u32;


#define BUFFER_SIZE  16   // Длина буфера может быть изменена
struct pec_ring_buffer {
    u32 payload_len;
    u8 *head;
    u8 *tail;
    u8 *payload_head;
    u8 *payload_tail;
    size_t size;
};


/*
   * Инициализировать кольцевой буфер
   * Кольцевой буфер может быть памятью, запрошенной malloc или флэш-накопителем
 * */
void pec_ring_buffer_init(struct pec_ring_buffer* dst, size_t size)
{
    dst->head = malloc(size);
    dst->payload_head = dst->payload_tail = dst->head;
    dst->tail = dst->head + size;
    dst->payload_len = 0;
    dst->size = size;
}

int pec_ring_buffer_read(struct pec_ring_buffer* src, u8* dst, u32 len)
{
    if(src->head == NULL) return -1;

    assert(dst);

    if(src->payload_len == 0) return 0;

    if(len > src->payload_len) len = src->payload_len;

    if(src->payload_head + len > src->tail)// нужно разделить на два раздела
    {
        int len1 = src->tail - src->payload_head;
        int len2 = len - len1;
        memcpy(dst, src->payload_head, len1);// Первый абзац
        memcpy(dst + len1, src->head, len2);// Второй абзац, перейти к началу всей области хранения
        src->payload_head = src->head + len2;// Обновляем начало используемого буфера
    }else
    {
        memcpy(dst, src->payload_head, len);
        src->payload_head = src->payload_head + len;// Обновляем начало используемого буфера
    }
    src->payload_len -= len;// Обновляем длину используемого буфера

    return len;
}

/*
   * функция: запись данных в буфер
   * param: указатель данных, написанный @src
   * @src_str_len длина записанных данных
   * return: -1: длина записи слишком велика
   * -2: буфер не инициализирован
 * */

int pec_ring_buffer_write(struct pec_ring_buffer* dst, const u8* src, u32 src_str_len)
{
    if(src_str_len > dst->size - dst->payload_len) {
        struct pec_ring_buffer new_buffer;
        pec_ring_buffer_init(&new_buffer, dst->size * 2);
        new_buffer.payload_len = dst->payload_len;
        pec_ring_buffer_read(dst, new_buffer.head, dst->payload_len);
        new_buffer.payload_tail += new_buffer.payload_len;
        free(dst->head);
        *dst = new_buffer;
    }

    // Копируем данные, которые будут сохранены в payload_tail
    if(dst->payload_tail + src_str_len > dst->tail)// нужно разделить на два раздела
    {
        int len1 = dst->tail - dst->payload_tail;
        int len2 = src_str_len - len1;
        memcpy(dst->payload_tail, src, len1);
        memcpy(dst->head, src + len1, len2);
        dst->payload_tail = dst->head + len2;// Указатель конца новой действительной области данных
    }else
    {
        memcpy(dst->payload_tail, src, src_str_len);
        dst->payload_tail += src_str_len;// Указатель конца новой действительной области данных
    }

    // Нужно пересчитать начальную позицию используемой области
    if(dst->payload_len + src_str_len > dst->size)
    {
        int move_len = dst->payload_len + src_str_len - dst->size;// Длина, на которую будет двигаться эффективный указатель
        if(dst->payload_head + move_len > dst->tail)// Его нужно разделить на два раздела для расчета
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

/*
   * функция: удалить данные из буфера
   * param: @dst: принять буфер для чтения данных
   * @len: длина данных для чтения
   * возврат: -1: без инициализации
   *> 0: фактическая длина чтения
 * */



int main() {
    char test_read_buf[100];
    struct pec_ring_buffer buf;
    pec_ring_buffer_init(&buf, 10);
    const char* test_string = "qwerty";
    pec_ring_buffer_write(&buf, test_string, strlen(test_string));
    pec_ring_buffer_read(&buf, test_read_buf, 4);
    pec_ring_buffer_write(&buf, test_string, strlen(test_string));
    pec_ring_buffer_write(&buf, test_string, strlen(test_string));
    return 0;
}