#include "store.h"

#include <linux/rhashtable.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>
#include <callsyms.h>

#define MAX_TRY_REGISTER_ENTITY 10000

static size_t proxy_id_counter = 0;
static size_t service_id_counter = 0;
static size_t file_id_counter = 0;

#define DEFAULT_BUF_SIZE 4096

files_node_t* new_file_node(struct filename* filename, uint64_t service_ID, bool service_registered) {
    files_node_t* node = vmalloc(sizeof(files_node_t));
    node->filename = filename;
    node->service_ID = service_ID;
    node->service_registered = service_registered;
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

void destroy_proxy_node(void* ptr, void* /*arg*/) {
    if (ptr == NULL)
        return;
    proxy_node_t* proxy_node = ptr;
    pec_ring_buffer_destroy(&proxy_node->stdin);
    pec_ring_buffer_destroy(&proxy_node->stdout);
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
        .key_offset = offsetof(service_node_t, ID),
        .head_offset = offsetof(service_node_t, head),
};

static struct rhashtable_params proxy_node_params = {
        .key_len = sizeof(uint32_t),
        .key_offset = offsetof(proxy_node_t , ID),
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

enum pec_store_error pec_store_register_file(pec_store_t* dst, struct filename *filename) {
    files_node_t *node = new_file_node(filename, 0, false);
    spin_lock(&dst->file_lock);
    node->file_ID = file_id_counter++;
    spin_unlock(&dst->file_lock);

    files_node_t *old_node = rhashtable_lookup_get_insert_fast(&dst->files, &node->head, file_node_params);
    if (old_node != NULL)
        return FILE_ALREADY_EXISTS;

    return OK;
}

enum pec_store_error pec_get_service_by_file(pec_store_t* src, struct filename *filename, uint64_t* service_id) {
    if (service_id == NULL)
        return BAD_ARG;
    spin_lock(&src->file_lock);
    files_node_t* node = rhashtable_lookup_fast(&src->files, filename, file_node_params);
    if (node == NULL)
        return FILE_NOT_FOUND;

    if (!node->service_registered)
        return SERVICE_NOT_REGISTERED;

    *service_id = node->service_ID;
    spin_unlock(&src->file_lock);
    return OK;
}

enum pec_store_error pec_store_associate_service_with_file(pec_store_t* dst, uint64_t service_ID, struct filename* filename, uint64_t *fileId) {

    spin_lock(&dst->file_lock);
    files_node_t* node = rhashtable_lookup_fast(&dst->files, filename, file_node_params);
    if (node == NULL) {
        spin_unlock(&dst->file_lock);
        return FILE_NOT_FOUND;
    }

    if (node->service_registered) {
        spin_unlock(&dst->file_lock);
        return FILE_ALREDY_ACCOCIATED;
    }
    uint64_t f_id = node->file_ID;


    spin_lock(&dst->service_lock);
    service_node_t* service_node = rhashtable_lookup_fast(&dst->services, &service_ID, service_node_params);
    if (service_node == NULL) {
        spin_unlock(&dst->service_lock);
        spin_unlock(&dst->file_lock);
        return SERVICE_NOT_REGISTERED;
    }
    node->service_ID = service_node->ID;
    node->service_registered = true;
    spin_unlock(&dst->service_lock);
    spin_unlock(&dst->file_lock);

    *fileId = f_id;
    return OK;
}

enum pec_store_error pec_store_get_service_by_file(pec_store_t* dst, struct filename* filename, service_node_t** sn) {
    spin_lock(&dst->file_lock);
    files_node_t* node = rhashtable_lookup_fast(&dst->files, filename, file_node_params);
    if (node == NULL) {
        spin_unlock(&dst->file_lock);
        return FILE_NOT_FOUND;
    }
    spin_unlock(&dst->file_lock);
    service_node_t* s =  rhashtable_lookup_fast(&dst->services, &node->service_ID, service_node_params);
    *sn = s;
    return OK;
}

enum pec_store_error pec_store_create_proxy(pec_store_t* dst, uint64_t service_ID, program_args_t* pa, uint64_t* proxy_id){
    spin_lock(&dst->service_lock);
    files_node_t* service_node = rhashtable_lookup_fast(&dst->services, &service_ID, service_node_params);
    if (service_node == NULL) {
        spin_unlock(&dst->service_lock);
        return SERVICE_NOT_REGISTERED;
    }
    size_t try_register_proxy = 0;
    RESTART_PROXY_REGISTER:;
    try_register_proxy++;
    spin_lock(&dst->proxy_lock);
    size_t proxy_ID  = proxy_id_counter++;
    spin_unlock(&dst->proxy_lock);

    proxy_node_t *proxy_node = vmalloc(sizeof(proxy_node_t));
    proxy_node->ID = proxy_ID;
    proxy_node->service_ID = service_ID;
    pec_ring_buffer_init(&proxy_node->stdin, DEFAULT_BUF_SIZE);
    pec_ring_buffer_init(&proxy_node->stdout, DEFAULT_BUF_SIZE);
    proxy_node->pa = pa;
    init_waitqueue_head(&proxy_node->proxy_get_recode_tasks);
    init_waitqueue_head(&proxy_node->proxy_read_tasks);
    init_waitqueue_head(&proxy_node->service_read_tasks);
    proxy_node_t * old_proxy_node = rhashtable_lookup_get_insert_fast(&dst->proxy, &proxy_node->head, proxy_node_params);
    if (old_proxy_node != NULL) {
        destroy_proxy_node(proxy_node, NULL);
        if (try_register_proxy >= MAX_TRY_REGISTER_ENTITY) {
            spin_unlock(&dst->service_lock);
            return UNABLE_TO_REGISTER_THE_PROXY;
        }
        goto RESTART_PROXY_REGISTER;
    }
    spin_unlock(&dst->service_lock);
    *proxy_id = proxy_node->ID;
    return OK;
}

enum pec_store_error pec_store_create_service(pec_store_t* dst, service_node_t** service_node_data) {
    service_node_t * service_node = vmalloc(sizeof(service_node_t));
    size_t service_register_counter = 0;
    RESTART_SERVICE_REGISTER:
    service_register_counter++;
    spin_lock(&dst->service_lock);
    service_node->ID  = proxy_id_counter++;
    spin_unlock(&dst->service_lock);
    service_node_t * old_service_node = rhashtable_lookup_get_insert_fast(&dst->services, &service_node->head, service_node_params);
    if (old_service_node != NULL) {
        if (service_register_counter >= MAX_TRY_REGISTER_ENTITY) {
            vfree(service_node);
            return UNABLE_TO_REGISTER_THE_SERVICE;
        }
        goto RESTART_SERVICE_REGISTER;
    }
    init_waitqueue_head(&service_node->service_get_proxy_task);
    *service_node_data = service_node;
    return OK;
}

enum pec_store_error pec_store_get_proxy_data(pec_store_t* dst, uint64_t proxy_ID, proxy_node_t** proxy_data) {
    proxy_node_t* n = rhashtable_lookup_fast(&dst->proxy, &proxy_ID, proxy_node_params);
    *proxy_data = n;
    return OK;
}

service_node_t* get_service_node_by_id(pec_store_t* dst, uint64_t service_id) {
    return rhashtable_lookup_fast(&dst->services, &service_id, service_node_params);
}