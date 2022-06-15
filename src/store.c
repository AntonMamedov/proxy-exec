#include "store.h"

#include <linux/rhashtable.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>
#include <callsyms.h>
static size_t file_id_counter = 0;

#define DEFAULT_BUF_SIZE 4096

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

static struct rhashtable_params service_node_params = {
        .key_len = sizeof(uint32_t),
        .key_offset = offsetof(service_node_t, PID),
        .head_offset = offsetof(service_node_t, head),
};

static struct rhashtable_params proxy_node_params = {
        .key_len = sizeof(uint32_t),
        .key_offset = offsetof(proxy_node_t , PID),
        .head_offset = offsetof(proxy_node_t , head),
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
    spin_lock_init(&dst->file_lock);
    return 0;
}

int pec_store_destroy(pec_store_t* dst) {
    rhashtable_free_and_destroy(&dst->files, destroy_file_node, NULL);
    return 0;
}

ssize_t pec_register_file(pec_store_t* dst, struct filename *filename) {
    spin_lock(&dst->file_lock);
    size_t current_file_id = file_id_counter++;
    spin_unlock(&dst->file_lock);
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

ssize_t pec_register_service(pec_store_t* dst, uint32_t PID, struct filename* filename) {
    service_node_t * node = vmalloc(sizeof(service_node_t));
    node->PID = PID;
    rhashtable_lookup_get_insert_fast(&dst->files, &node->head, service_node_params);
    spin_lock(&dst->file_lock);
    files_node_t* file_node = rhashtable_lookup_fast(&dst->files, filename, file_node_params);
    file_node->service_PID = PID;
    spin_unlock(&dst->file_lock);
    return 0;
}

bool pec_check_service(pec_store_t* src, uint32_t PID) {
    spin_lock(&src->service_lock);
    service_node_t * node = rhashtable_lookup_fast(&src->services, &PID, service_node_params);
    bool exist = node != NULL;
    spin_unlock(&src->service_lock);
    return exist;
}

ssize_t pec_register_proxy(pec_store_t* dst, uint32_t PID, struct filename* filename) {
    spin_lock(&dst->file_lock);
    files_node_t* file_node = rhashtable_lookup_fast(&dst->files, filename, file_node_params);
    if (file_node == NULL){
        spin_unlock(&dst->file_lock);
        return -1;
    }
    uint32_t service_PID = file_node->service_PID;
    spin_unlock(&dst->file_lock);
    proxy_node_t * proxy_node = vmalloc(sizeof(proxy_node_t));
    proxy_node->PID = PID;
    proxy_node->service_PID = service_PID;
    proxy_node->task = current;
    pec_ring_buffer_init(&proxy_node->stdin, DEFAULT_BUF_SIZE);
    pec_ring_buffer_init(&proxy_node->stdout, DEFAULT_BUF_SIZE);
    proxy_node_t* old_node = rhashtable_lookup_get_insert_fast(&dst->proxy, &proxy_node->head, proxy_node_params);
    if (old_node != NULL) {
        vfree(proxy_node);
        return -1;
    return -0;
}

