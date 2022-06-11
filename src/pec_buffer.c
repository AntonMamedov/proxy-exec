#include "pec_buffer.h"

#include <linux/kernel.h>

ssize_t pec_buffer_write (byte_t* dst, size_t dst_size,
                          const byte_t* src, size_t src_size,
                            size_t current_read_pos, size_t* current_write_pos) {
}

ssize_t pec_buffer_service_write(const byte_t* b, size_t size) {

}

ssize_t pec_buffer_service_read(byte_t *desc, size_t size) {

}

