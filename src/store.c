#include "store.h"

#include <linux/rhashtable.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>
#include <callsyms.h>
static size_t file_id_counter = 0;

files_node_t* new_file_node(struct filename* filename, uint64_t ID, uint32_t service_PID) {
    files_node_t* node = vmalloc(sizeof(files_node_t));
    node->filename = filename;
    node->ID = ID;
    node->service_PID = service_PID;
    return node;
}

void destroy_file_node(void* ptr, void* /*arg*/) {
    if (ptr == NULL)
        return;
    files_node_t* node = ptr;
    if (node->filename != NULL) {
        void (*putname)(struct filename *name) = get_callsym_by_name("putname");
        putname(node->filename);
    }
    vfree(ptr);
}

int file_node_cmp(struct rhashtable_compare_arg *arg,
                                const void *obj) {
    return strcmp(((struct filename*)arg->key)->name,((files_node_t*)obj)->filename->name);
}

u32 filename_hash(const void *data, u32 len, u32 seed)
{
    const char* str = ((struct filename*)data)->name;
    const int p = 53;
    const int m = 912239431;
    u32 hash_value = 0;
    long long p_pow = 1;
    size_t i;
    for (i = 0; str[i] != 0; i++) {
        hash_value = (hash_value + (str[i] - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return hash_value;
}


u32 file_node_hash(const void *data, u32 len, u32 seed) {
    return filename_hash((files_node_t*)data, len, seed);
}

static struct rhashtable_params file_node_params = {
    .key_len = 0,
    .key_offset = offsetof(files_node_t, filename),
    .head_offset = offsetof(files_node_t, head),
    .obj_cmpfn = file_node_cmp,
    .obj_hashfn = file_node_hash,
    .hashfn = filename_hash,
    .min_size = 16
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
    spin_lock_init(&dst->lock);
    return 0;
}

int pec_store_destroy(pec_store_t* dst) {
    rhashtable_free_and_destroy(&dst->files, destroy_file_node, NULL);
    return 0;
}

ssize_t pec_register_file(pec_store_t* dst, struct filename *filename) {
    spin_lock(&dst->lock);
    size_t current_file_id = file_id_counter++;
    spin_unlock(&dst->lock);
    files_node_t *node = new_file_node(filename, current_file_id, -2);
    files_node_t *old_node = rhashtable_lookup_get_insert_fast(&dst->files, &node->head, file_node_params);
    if (old_node != NULL)
        return -1;
    return 0;
}

int64_t pec_check_file(pec_store_t* src, struct filename *filename) {
    files_node_t* node = rhashtable_lookup_fast(&src->files, filename, file_node_params);
    if (node == NULL)
        return -1;

    return node->service_PID;
}