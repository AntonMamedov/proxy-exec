#include "store.h"
#define PEC_STORE_LINUX_KERNEL_BUILD
#ifdef PEC_STORE_KERNEL
#include <linux/slab.h>
#include <linux/types.h>
#else
#ifdef __cplusplus
#include <atomic>
  using std::atomic_long;
  using std::memory_order;
  using std::memory_order_acquire;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */
#define GFP_KERNEL 0
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
typedef struct atomic_long__{
    atomic_long val;
} atomic_long_t;
void atomic_long_set(atomic_long_t* dst, long val) { dst->val = val;}
long  atomic_long_read(const atomic_long_t* dst) { return dst->val;}
void atomic_long_inc(atomic_long_t* dst) { dst->val += 1;}
typedef pthread_rwlock_t rwlock_t;
#define kmalloc(__len, __mod) malloc(__len)
#define kfree(__obj) free(__obj)
#define rwlock_init(__lock) pthread_rwlock_init(__lock, NULL)
#define read_lock(__lock) pthread_rwlock_rdlock(__lock)
#define write_lock(__lock) pthread_rwlock_wrlock(__lock)
#define read_unlock(__lock) pthread_rwlock_unlock(__lock)
#define write_unlock(__lock) pthread_rwlock_unlock(__lock)
#endif


typedef int (*pec_map_key_cmp_p)(const void*, const void*);
typedef void* (*pec_map_node_make_p)(const void*, void*);
typedef size_t (*hash_p)(const void*, size_t size);
typedef const void* (*get_key_p)(const void*);
typedef void* (*get_value_p)(void*);
typedef void (*destroy_node_p)(void*);
typedef void (*on_search_and_change_p)(void* /* node data */, void* /*data*/);
typedef void (*on_search_p)(const void* /* node data */, void* /*data*/);

typedef struct pec_store_map_operations {
    pec_map_key_cmp_p cmp;
    pec_map_node_make_p make;
    hash_p hash;
    get_key_p get_key;
    get_value_p get_value;
    destroy_node_p destroy_node;
} pec_store_map_operations_t;

typedef struct pec_store_map_bucket {
    struct pec_store_map_bucket* next;
    rwlock_t rwlock;
    void* data;
} pec_store_map_bucket_t;

struct pec_store_map {
    rwlock_t* rwlock;            // general lock
    pec_store_map_bucket_t** table;
    rwlock_t* node_rwlock;       // cell locks
    atomic_long_t size;
    size_t allocated_size;
    size_t load_factor;
    const pec_store_map_operations_t* ops;
};

static ssize_t pec_store_map_init(pec_store_map_t* dst, size_t allocated_size, size_t load_factor, const pec_store_map_operations_t* ops, rwlock_t* general_lock) {
    if (allocated_size % 2 == 0)
        allocated_size += 1;

    dst->table = (pec_store_map_bucket_t**)kmalloc(sizeof(pec_store_map_bucket_t*) * allocated_size, GFP_KERNEL);
    if (dst->table == NULL) {
        return -1;
    }
    memset(dst->table, 0, sizeof(pec_store_map_bucket_t*) * allocated_size);
    dst->node_rwlock = (rwlock_t*)kmalloc(sizeof(rwlock_t) * allocated_size, GFP_KERNEL);
    if (dst->table == NULL) {
        kfree(dst->table);
        return -1;
    }
    size_t i;
    for (i = 0; i < allocated_size; i++)
        rwlock_init(&dst->node_rwlock[i]);
    if (general_lock == NULL)
        dst->rwlock = (rwlock_t*)kmalloc(sizeof(rwlock_t), GFP_KERNEL);
    else
        dst->rwlock = general_lock;

    if (dst->table == NULL) {
        kfree(dst->table);
        kfree(dst->node_rwlock);
        return -1;
    }

    dst->allocated_size = allocated_size;
    atomic_long_set(&dst->size, 0);
    dst->ops = ops;
    dst->load_factor = load_factor;
    return (ssize_t)allocated_size;
}

static int pec_store_map_release(pec_store_map_t* dst) {
    kfree(dst->rwlock);
    kfree(dst->node_rwlock);
    size_t i;
    for (i = 0; i < dst->allocated_size; i++)
        dst->ops->destroy_node(dst->table[i]);
    kfree(dst->table);
    return 0;
}

void empty_on_search(void* node_data, void* data){
    (void)node_data;
    (void)data;
}

typedef enum {
    PEC_DATA_READ,
    PEC_DATA_WRITE
} pec_search_mod_t;

static pec_store_map_bucket_t* pec_store_map_search_impl(const pec_store_map_t* src, const void* key, on_search_and_change_p on_search_fn, void* data, pec_search_mod_t mod) {
    read_lock(src->rwlock);
    size_t index = src->ops->hash(key, src->allocated_size);
    read_lock(&src->node_rwlock[index]);
    pec_store_map_bucket_t* cur = src->table[index];
    if (cur != NULL) {
        while (cur != NULL && src->ops->cmp(src->ops->get_key(cur), key) != 0)
            cur = cur->next;
    }
    if (cur != NULL) {
        switch (mod) {
            case PEC_DATA_READ:
                read_lock(&cur->rwlock);
                on_search_fn(src->ops->get_value(cur->data), data);
                read_unlock(&cur->rwlock);
                break;
            case PEC_DATA_WRITE:
                write_lock(&cur->rwlock);
                on_search_fn(src->ops->get_value(cur->data), data);
                write_unlock(&cur->rwlock);
                break;
        }
    }

    read_unlock(&src->node_rwlock[index]);
    read_unlock(src->rwlock);

    return cur;
}

static bool pec_store_map_contain(const pec_store_map_t* src, const void* key) {
    if (pec_store_map_search_impl(src, key, empty_on_search, NULL, PEC_DATA_READ) == NULL)
        return false;

    return true;
}

static int pec_store_map_search(const pec_store_map_t* src, const void* key, on_search_p on_search_fn, void* data) {
    if (pec_store_map_search_impl(src, key, (on_search_and_change_p)on_search_fn, data, PEC_DATA_READ) == NULL) {
        return -1;
    }

    return 0;
}

static int pec_store_map_search_and_change(const pec_store_map_t* src, const void* key, on_search_and_change_p on_search_fn, void* data) {
    if (pec_store_map_search_impl(src, key, on_search_fn, data, PEC_DATA_WRITE) == NULL) {
        return -1;
    }

    return 0;
}

static void pec_store_map_insert_bucket(pec_store_map_t* dst, pec_store_map_bucket_t* bucket) {
    if (bucket == NULL)
        return;

    pec_store_map_bucket_t* cur = bucket;
    while (cur != NULL) {
        pec_store_map_bucket_t* inserted_bucket = cur;
        cur = cur->next;
        inserted_bucket->next = NULL;
        size_t index = dst->ops->hash(dst->ops->get_key(inserted_bucket->data), dst->allocated_size);

        if (dst->table[index] == NULL) {
            dst->table[index] = inserted_bucket;
        } else {
            pec_store_map_bucket_t* cur_bucket = dst->table[index];
            while (cur_bucket->next != NULL)
                cur_bucket = cur_bucket->next;
            cur_bucket->next = inserted_bucket;
        }
    }
}

static int pec_store_map_insert(pec_store_map_t* dst, const void* key, void* value) {
    if (dst->allocated_size / atomic_long_read(&dst->size) >= dst->load_factor) {
        write_lock(dst->rwlock);
        pec_store_map_t new_map;
        if (pec_store_map_init(&new_map, dst->allocated_size * 2, dst->load_factor, dst->ops, dst->rwlock) < 0) {
            write_unlock(dst->rwlock);
            return -1;
        }

        size_t i;
        for (i = 0; i < dst->allocated_size; i++) {
            pec_store_map_insert_bucket(&new_map, dst->table[i]);
        }
#ifdef PEC_STORE_LINUX_KERNEL_BUILD
        new_map.size = dst->size;
#else
        atomic_long_set(&new_map.size, dst->size.val);
#endif
        kfree(dst->node_rwlock);
        kfree(dst->table);
        write_unlock(dst->rwlock);
    }

    size_t index = dst->ops->hash(key, dst->allocated_size);
    read_lock(dst->rwlock);
    write_lock(&dst->node_rwlock[index]);
    pec_store_map_bucket_t* new_node = NULL;
    if (dst->table[index] == NULL) {
        dst->table[index] = (pec_store_map_bucket_t*)kmalloc(sizeof(pec_store_map_bucket_t), GFP_KERNEL);
        if (dst->table[index] == NULL) {
            return -1;
        }
        new_node = dst->table[index];
    } else {
        pec_store_map_bucket_t* cur = dst->table[index];
        while (cur->next != NULL) {
            cur = cur -> next;
            if (dst->ops->cmp(dst->ops->get_key(cur->data), key) == 0)
                return -2;
        }
        cur->next = (pec_store_map_bucket_t*)kmalloc(sizeof(pec_store_map_bucket_t), GFP_KERNEL);
        new_node = cur;
    }
    new_node->next = NULL;
    new_node->data = dst->ops->make(key, value);
    write_unlock(&dst->node_rwlock[index]);
    read_unlock(dst->rwlock);
    atomic_long_inc(&dst->size);
    return 0;
}

//// cmp func for file_to_file_id map
//static int str_key_comparator(const void* l, const void* r) {
//    return strcmp((const char*)l, (const char*)r);
//}
//
////cmp func for service_to_file_id, file_to_proxy, proxy_to_service maps
//static int size_t_key_comparator(const void* l, const void* r) {
//    return (int)*(size_t*)l - (int)*(size_t*)r;
//}
//
//static void* file_to_file_id_node_make(const void* key, void* value) {
//
//}
//
//int pec_store_init(pec_store_t* dst) {
//
//}

//static size_t create_file_id(const unsigned char* file) {
//    size_t ID = 0;
//    size_t i;
//    for (i = 0; i < strlen((const char*)file); i++) {
//        ID += file[i];
//    }
//    return ID;
//}
//
//int pec_set_filepath_and_generate_file_id(pec_store_t* dst, const unsigned char* file) {
//    size_t* ID = kmalloc(sizeof(size_t), GFP_KERNEL);
//    *ID = create_file_id(file);
//    return pec_store_map_insert(dst->files, file, ID);
//}
//
//
//int pec_register_service(pec_store_t* dst, size_t pid /* service PID */, size_t ID /* file ID */) {
//    pec_service_data_t * data = kmalloc(sizeof (pec_service_data_t), GFP_KERNEL);
//    data->PID = ID;
//    data->head = NULL;
//    return pec_store_map_insert(dst->service_to_file_id, &pid, data);
//}
//
//static void pec_register_proxy_search(const void* node_data, void* data) {
//    const size_t *ID = (const size_t*)node_data;
//    *((size_t*)data) = *ID;
//}
//
//int pec_register_proxy(pec_store_t* dst, const char *file) {
//    size_t ID = 0;
//    if (pec_store_map_search(dst->files, file, pec_register_proxy_search, &ID) < 0)
//        return -1;
//    return pec_store_map_insert(dst->file_to_proxy, &ID, NULL);
//}
//
//pec_service_data_t* pec_get_service_data(pec_store_t* dst, size_t pid) {
//    return (pec_service_data_t*) pec_store_map_search(dst->service_to_file_id, &pid);
//}
//
