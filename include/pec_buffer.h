#include <linux/types.h>

#define PEC_RCVBUF 4096
#define PEC_SNDBUF 4096

typedef uint8_t byte_t;

struct pec_buffer {
    size_t service_buffer_read_pos;
    size_t service_write_pos;
    size_t pp_read_pos;
    size_t ppp_write_pos;
    byte_t service_buffer[PEC_RCVBUF];
    byte_t pp_buffer[PEC_SNDBUF];     // pp - proxy process
};

typedef struct pec_buffer pec_buffer;

ssize_t pec_buffer_service_write(const byte_t* b, size_t size);
ssize_t pec_buffer_service_read(byte_t *desc, size_t size);

