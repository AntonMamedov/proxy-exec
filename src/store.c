#include "store.h"

#include <linux/rhashtable.h>
#include <linux/vmalloc.h>


files_node_t* new_file_node(uint64_t ID, uint32_t service_PID) {
    files_node_t* node = vmalloc(sizeof(files_node_t));
    node->ID = ID;
    node->service_PID = service_PID;
    return node;
}

void destroy_file_node(void* ptr, void* /*arg*/) {
    vfree(ptr);
}

static struct rhashtable_params file_node_params = {
    .key_len = sizeof(uint32_t),
    .key_offset = offsetof(files_node_t, ID),
    .head_offset = offsetof(files_node_t, head),
};


static struct rhashtable new_files_table (int* ret) {
    struct rhashtable ht;
    *ret = rhashtable_init(&ht, &file_node_params);
    return ht;
}

int pec_store_init(pec_store_t* dst) {
    int ret = 0;
    dst->files = new_files_table(&ret);
    if (ret != 0)
        return ret;

    return 0;
}

int pec_store_destroy(pec_store_t* dst) {
    rhashtable_free_and_destroy(&dst->files, destroy_file_node, NULL);
    return 0;
}

ssize_t pec_register_file(pec_store_t* dst, uint32_t ID) {
    files_node_t *node = new_file_node(ID, -1);
    files_node_t *old_node = rhashtable_lookup_get_insert_fast(&dst->files, &node->head, file_node_params);
    if (old_node != NULL)
        return -1;
    return 0;
}

