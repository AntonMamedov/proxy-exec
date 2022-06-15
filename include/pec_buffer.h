#include <linux/types.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#define PEC_RCVBUF 4096
#define PEC_SNDBUF 4096

#define EXTEND_FACTOR 4

typedef unsigned char u8;
typedef unsigned int u32;


struct pec_ring_buffer {
    u32 payload_len;
    u8 *head;
    u8 *tail;
    u8 *payload_head;
    u8 *payload_tail;
    size_t size;
};

void pec_ring_buffer_init(struct pec_ring_buffer* dst, size_t size);
int pec_ring_buffer_read(struct pec_ring_buffer* src, u8* dst, u32 len);
int pec_ring_buffer_write(struct pec_ring_buffer* dst, const u8* src, u32 src_str_len);
void  pec_ring_buffer_destroy(struct pec_ring_buffer* dst) ;