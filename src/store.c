#include "store.h"

#ifndef PEC_STORE_TEST_BUILD
#include <linux/vmalloc.h>
#else
#include <stdlib.h>
#include <malloc.h>
#define vmalloc(__size) malloc(__size)
#define vzalloc(__size) calloc(__size, 1)
#define vfree(__obj) free(__obj)
#endif



size_t default_string_hash(const void* str) {
    const int p = 53;
    const int m =  321321981;
    size_t hash_value = 0;
    size_t p_pow = 1;
    size_t i = 0;
    const char* cast_str = ((const char*)str);
    while (cast_str[i] != '\0') {
        hash_value = (hash_value + (cast_str[i] - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
        i++;
    }
    return hash_value;
}

int pec_list_push_back_with_duplicate_check(pec_list_t* dst, void* data, int (*cmp)(const void*, const void*), const void* (*get)(const void*)) {
    if (dst == NULL || data == NULL)
        return -PEC_BAD_ARG;
    if (dst->head == NULL) {
        dst->head = vmalloc(sizeof(pec_list_node_t));
        if (dst->head == NULL)
            return -PEC_ERROR_MEM_ALLOC;
        dst->head->next = NULL;
        dst->tail = dst->head;
        dst->head->data = data;
        return 0;
    }
    pec_list_node_t* cur = dst->head;
    while (cur != NULL) {
        if (cmp(get(data), get(cur->data)) == 0)
            return -PEC_DUPLICATE;
        cur = cur->next;
    }
    dst->tail->next = vmalloc(sizeof(pec_list_node_t));
    if (dst->tail->next == NULL)
        return -PEC_ERROR_MEM_ALLOC;
    dst->tail = dst->tail->next;
    dst->tail->next = NULL;
    dst->tail->data = data;
    return 0;
}

bool pec_list_delete_first(pec_list_t* dst, void* data, bool(*fn)(void*, void*)) {
    if (dst == NULL || data == NULL || fn == NULL)
        return -PEC_BAD_ARG;
    pec_list_node_t* cur = dst->head;
    pec_list_node_t* prev = NULL;
    bool is_deleted = false;
    while (cur != NULL) {
        if (fn(data, cur->data)) {
            if (prev == NULL) {
                dst->head = cur->next;
                if (dst->head == NULL)
                    dst->tail = NULL;
            } else
                prev->next = cur->next;

            vfree(cur);
            is_deleted = true;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    return is_deleted;
}

void pec_list_push_back_node(pec_list_t* dst, pec_list_node_t* node) {
    if (dst == NULL || node == NULL)
        return;
    node->next = NULL;
    if (dst->head == NULL) {
        dst->head = node;
        dst->tail = dst->head;
        return;
    }
    dst->tail->next = node;
    dst->tail = dst->tail->next;
}

pec_list_node_t* pec_list_pop_front_node(pec_list_t* dst) {
    if (dst->head == NULL)
        return NULL;
    pec_list_node_t* cur_head = dst->head;
    dst->head = dst->head->next;
    if (dst->head == NULL)
        dst->tail = NULL;
    cur_head->next = NULL;
    return cur_head;
}

void pec_list_destroy(pec_list_t* dst, void (*fn)(void*)) {
    if (dst == NULL || fn == NULL)
        return;
    pec_list_node_t* cur = dst->head;
    while (cur != NULL) {
        fn(cur->data);
        pec_list_node_t* prev = cur;
        cur = cur->next;
        vfree(prev);
    }
}

int pec_hashtable_init(pec_hashtable_t* dst, size_t size, size_t load_factor_major, size_t load_factor_minor, const pec_hashtable_operation_t* ops) {
    if (dst == NULL || ops == NULL)
        return -PEC_BAD_ARG;
    dst->buckets = vzalloc(sizeof(pec_list_t) * size);
    if (dst->buckets == NULL)
        return -PEC_ERROR_MEM_ALLOC;

    dst->size = size;
    dst->load_factor_major = load_factor_major;
    dst->load_factor_minor = load_factor_minor;
    dst->ops = ops;
    dst->load = 0;
    return 0;
}

int pec_hashtable_destroy(pec_hashtable_t* dst) {
    if (dst == NULL)
        return -PEC_BAD_ARG;
    size_t i;
    for (i = 0; i < dst->size; i++)
        pec_list_destroy(&dst->buckets[i], dst->ops->on_node_destroy);
    vfree(dst->buckets);
    return 0;
}

static int pec_hashtable_extend(pec_hashtable_t* dst, void* external_data) {
    dst->ops->on_extend_start(dst, external_data);
    size_t new_size = dst->size * (dst->load_factor_major + 1);
    new_size = new_size % 2 == 0 ? new_size + 1 : new_size;
    pec_hashtable_t new_hashtable;
    pec_hashtable_init(&new_hashtable, new_size, dst->load_factor_major, dst->load_factor_minor, dst->ops);
    size_t i;
    for (i = 0; i < dst->size; i++) {
        pec_list_node_t* node;
        for (node = pec_list_pop_front_node(&dst->buckets[i]); node != NULL; node = pec_list_pop_front_node(&dst->buckets[i])) {
            size_t h = new_hashtable.ops->hash(new_hashtable.ops->get_key(node->data));
            size_t index = h % new_hashtable.size;
            pec_list_push_back_node(&new_hashtable.buckets[index], node);
        }
    }
    new_hashtable.load = dst->load;
    new_hashtable.ops->on_extend_end(&new_hashtable, external_data);
    *dst = new_hashtable;
    return 0;
}

int pec_hashtable_inset(pec_hashtable_t* dst, void* data, void* external_data) {
    if (dst->load != 0) {
        if (dst->size / dst->load > dst->load_factor_major
            || (dst->size / dst->load == dst->load_factor_major && dst->size % dst->load >= dst->load_factor_minor)) {
            pec_hashtable_extend(dst, external_data);
        }
    }
    size_t h = dst->ops->hash(dst->ops->get_key(data));
    size_t index = h % dst->size;
    dst->ops->on_insert_start(dst, index, external_data);
    int ret = pec_list_push_back_with_duplicate_check(&dst->buckets[index], data, dst->ops->key_cmp, dst->ops->get_key);
    dst->ops->on_insert_end(dst, index, external_data);
    return ret;
}

int pec_hashtable_search(const pec_hashtable_t *src, void* key, void* external_data, void (*fn)(void*, void*)) {
    src->ops->on_read_start(src, src->size, external_data);
    size_t h = src->ops->hash(key);
    size_t index = h % src->size;
}


//#ifndef PEC_STORE_TEST_BUILD
//#include <linux/slab.h>
//#include <linux/types.h>
//#include <linux/vmalloc.h>

//#else
//#ifdef __cplusplus
//#include <atomic>
//  using std::atomic_long;
//  using std::memory_order;
//  using std::memory_order_acquire;
//#else /* not __cplusplus */
//#include <stdatomic.h>
//#endif /* __cplusplus */
//#define GFP_KERNEL 0
//#include <stdlib.h>
//#include <pthread.h>
//#include <string.h>
//#include <stdbool.h>
//typedef struct atomic_long__{
//    atomic_long val;
//} atomic_long_t;
//void atomic_long_set(atomic_long_t* dst, long val) { dst->val = val;}
//long  atomic_long_read(const atomic_long_t* dst) { return dst->val;}
//void atomic_long_inc(atomic_long_t* dst) { dst->val += 1;}
//typedef pthread_rwlock_t rwlock_t;
//#define vzalloc(__len) malloc(__len)
//#define vfree(__obj) free(__obj)
//#define rwlock_init(__lock) pthread_rwlock_init(__lock, NULL)
//#define read_lock(__lock) pthread_rwlock_rdlock(__lock)
//#define write_lock(__lock) pthread_rwlock_wrlock(__lock)
//#define read_unlock(__lock) pthread_rwlock_unlock(__lock)
//#define write_unlock(__lock) pthread_rwlock_unlock(__lock)
//
//#endif
//
//#define PEC_ERROR_MEM_ALLOCATED 1
//
//typedef int (*pec_map_key_cmp_p)(const void*, const void*);
//typedef void* (*pec_map_node_make_p)(const void*, void*);
//typedef size_t (*hash_p)(const void*, size_t size);
//typedef const void* (*get_key_p)(const void*);
//typedef void* (*get_value_p)(void*);
//typedef void (*destroy_node_data_p)(void*);
//typedef void (*on_search_and_change_p)(void* /* node data */, void* /*data*/);
//typedef void (*on_search_p)(const void* /* node data */, void* /*data*/);
//
//typedef struct pec_store_map_operations {
//    pec_map_key_cmp_p cmp;
//    pec_map_node_make_p make;
//    hash_p hash;
//    get_key_p get_key;
//    get_value_p get_value;
//    destroy_node_data_p destroy_node;
//} pec_store_map_operations_t;
//
//typedef struct pec_store_map_node {
//    struct pec_store_map_node* next;
//    rwlock_t rwlock;
//    void* data;
//} pec_store_map_node_t;
//
//struct pec_hashtable {
//    rwlock_t* rwlock;            // general lock
//    pec_store_map_node_t** buckets;
//    rwlock_t* node_rwlock;       // cell locks
//    atomic_long_t size;
//    size_t allocated_size;
//    size_t load_factor;
//    const pec_store_map_operations_t* ops;
//};
//
//static ssize_t pec_store_map_init(pec_hashtable_t* dst, size_t allocated_size, size_t load_factor, const pec_store_map_operations_t* ops, rwlock_t* general_lock) {
//    if (allocated_size % 2 == 0)
//        allocated_size += 1;
//
//    // allocate buckets
//    dst->buckets = (pec_store_map_node_t**)vzalloc(sizeof(pec_store_map_node_t*) * allocated_size);
//    if (dst->buckets == NULL)
//        return -PEC_ERROR_MEM_ALLOCATED;
//
//    //allocate cell locks
//    dst->node_rwlock = (rwlock_t*)vzalloc(sizeof(rwlock_t) * allocated_size);
//    if (dst->buckets == NULL) {
//        vfree(dst->buckets);
//        return -PEC_ERROR_MEM_ALLOCATED;
//    }
//
//    // init cell locks
//    size_t i;
//    for (i = 0; i < allocated_size; i++)
//        rwlock_init(&dst->node_rwlock[i]);
//
//    if (general_lock == NULL)
//        dst->rwlock = (rwlock_t*)vzalloc(sizeof(rwlock_t));
//    else
//        dst->rwlock = general_lock;
//
//    if (dst->rwlock == NULL) {
//        vfree(dst->buckets);
//        vfree(dst->node_rwlock);
//        return -1;
//    }
//
//    dst->allocated_size = allocated_size;
//    atomic_long_set(&dst->size, 0);
//    dst->ops = ops;
//    dst->load_factor = load_factor;
//    return (ssize_t)allocated_size;
//}
//
//static int pec_store_map_release(pec_hashtable_t* dst) {
//    vfree(dst->rwlock);
//    vfree(dst->node_rwlock);
//    size_t i;
//    for (i = 0; i < dst->allocated_size; i++) {
//        pec_store_map_node_t* cur = dst->buckets[i];
//        while (cur != NULL) {
//            pec_store_map_node_t* next = cur;
//            dst->ops->destroy_node(cur->data);
//            vfree(cur);
//            cur = next;
//        }
//    }
//    vfree(dst->buckets);
//    return 0;
//}
//
//void empty_on_search(void* node_data, void* data){
//    (void)node_data;
//    (void)data;
//}
//
//typedef enum {
//    PEC_DATA_READ,
//    PEC_DATA_WRITE
//} pec_search_mod_t;
//
//static pec_store_map_node_t* pec_store_map_search_impl(const pec_hashtable_t* src, const void* key, on_search_and_change_p on_search_fn, void* data, pec_search_mod_t mod) {
//    read_lock(src->rwlock);
//    size_t index = src->ops->hash(key, src->allocated_size);
//    read_lock(&src->node_rwlock[index]);
//    pec_store_map_node_t* cur = src->buckets[index];
//    if (cur != NULL) {
//        while (cur != NULL && src->ops->cmp(src->ops->get_key(cur), key) != 0)
//            cur = cur->next;
//    }
//    if (cur != NULL) {
//        switch (mod) {
//            case PEC_DATA_READ:
//                read_lock(&cur->rwlock);
//                on_search_fn(src->ops->get_value(cur->data), data);
//                read_unlock(&cur->rwlock);
//                break;
//            case PEC_DATA_WRITE:
//                write_lock(&cur->rwlock);
//                on_search_fn(src->ops->get_value(cur->data), data);
//                write_unlock(&cur->rwlock);
//                break;
//        }
//    }
//
//    read_unlock(&src->node_rwlock[index]);
//    read_unlock(src->rwlock);
//
//    return cur;
//}
//
//static bool pec_store_map_contain(const pec_hashtable_t* src, const void* key) {
//    if (pec_store_map_search_impl(src, key, empty_on_search, NULL, PEC_DATA_READ) == NULL)
//        return false;
//
//    return true;
//}
//
//static int pec_store_map_search(const pec_hashtable_t* src, const void* key, on_search_p on_search_fn, void* data) {
//    if (pec_store_map_search_impl(src, key, (on_search_and_change_p)on_search_fn, data, PEC_DATA_READ) == NULL) {
//        return -1;
//    }
//
//    return 0;
//}
//
//static int pec_store_map_search_and_change(const pec_hashtable_t* src, const void* key, on_search_and_change_p on_search_fn, void* data) {
//    if (pec_store_map_search_impl(src, key, on_search_fn, data, PEC_DATA_WRITE) == NULL) {
//        return -1;
//    }
//
//    return 0;
//}
//
//static void pec_store_map_insert_bucket(pec_hashtable_t* dst, pec_store_map_node_t* bucket) {
//    if (bucket == NULL)
//        return;
//
//    pec_store_map_node_t* cur = bucket;
//    while (cur != NULL) {
//        pec_store_map_node_t* inserted_bucket = cur;
//        cur = cur->next;
//        inserted_bucket->next = NULL;
//        size_t index = dst->ops->hash(dst->ops->get_key(inserted_bucket->data), dst->allocated_size);
//
//        if (dst->buckets[index] == NULL) {
//            dst->buckets[index] = inserted_bucket;
//        } else {
//            pec_store_map_node_t* cur_bucket = dst->buckets[index];
//            while (cur_bucket->next != NULL)
//                cur_bucket = cur_bucket->next;
//            cur_bucket->next = inserted_bucket;
//        }
//    }
//}
//
//static int pec_store_map_insert(pec_hashtable_t* dst, const void* key, void* value) {
//    if (atomic_long_read(&dst->size) / dst->allocated_size >= dst->load_factor) {
//        write_lock(dst->rwlock);
//        pec_hashtable_t new_map;
//        if (pec_store_map_init(&new_map, dst->allocated_size * 2, dst->load_factor, dst->ops, dst->rwlock) < 0) {
//            write_unlock(dst->rwlock);
//            return -1;
//        }
//
//        size_t i;
//        for (i = 0; i < dst->allocated_size; i++) {
//            pec_store_map_insert_bucket(&new_map, dst->buckets[i]);
//        }
//#ifndef PEC_STORE_TEST_BUILD
//        new_map.size = dst->size;
//#else
//        atomic_long_set(&new_map.size, dst->size.val);
//#endif
//        vfree(dst->node_rwlock);
//        vfree(dst->buckets);
//        write_unlock(dst->rwlock);
//    }
//
//    size_t index = dst->ops->hash(key, dst->allocated_size);
//    read_lock(dst->rwlock);
//    write_lock(&dst->node_rwlock[index]);
//    pec_store_map_node_t* new_node = NULL;
//    if (dst->buckets[index] == NULL) {
//        dst->buckets[index] = (pec_store_map_node_t*)vzalloc(sizeof(pec_store_map_node_t));
//        if (dst->buckets[index] == NULL) {
//            return -1;
//        }
//        new_node = dst->buckets[index];
//    } else {
//        pec_store_map_node_t* cur = dst->buckets[index];
//        while (cur->next != NULL) {
//            cur = cur -> next;
//            if (dst->ops->cmp(dst->ops->get_key(cur->data), key) == 0)
//                return -2;
//        }
//        cur->next = (pec_store_map_node_t*)vzalloc(sizeof(pec_store_map_node_t));
//        new_node = cur;
//    }
//    new_node->next = NULL;
//    new_node->data = dst->ops->make(key, value);
//    write_unlock(&dst->node_rwlock[index]);
//    read_unlock(dst->rwlock);
//    atomic_long_inc(&dst->size);
//    return 0;
//}
//
////// cmp func for file_to_file_id map
////static int str_key_comparator(const void* l, const void* r) {
////    return strcmp((const char*)l, (const char*)r);
////}
////
//////cmp func for service_to_file_id, file_to_proxy, proxy_to_service maps
////static int size_t_key_comparator(const void* l, const void* r) {
////    return (int)*(size_t*)l - (int)*(size_t*)r;
////}
////
////static void* file_to_file_id_node_make(const void* key, void* value) {
////
////}
////
////int pec_store_init(pec_store_t* dst) {
////
////}
//
////static size_t create_file_id(const unsigned char* file) {
////    size_t ID = 0;
////    size_t i;
////    for (i = 0; i < strlen((const char*)file); i++) {
////        ID += file[i];
////    }
////    return ID;
////}
////
////int pec_set_filepath_and_generate_file_id(pec_store_t* dst, const unsigned char* file) {
////    size_t* ID = kmalloc(sizeof(size_t), GFP_KERNEL);
////    *ID = create_file_id(file);
////    return pec_store_map_insert(dst->files, file, ID);
////}
////
////
////int pec_register_service(pec_store_t* dst, size_t pid /* service PID */, size_t ID /* file ID */) {
////    pec_service_data_t * data = kmalloc(sizeof (pec_service_data_t), GFP_KERNEL);
////    data->PID = ID;
////    data->head = NULL;
////    return pec_store_map_insert(dst->service_to_file_id, &pid, data);
////}
////
////static void pec_register_proxy_search(const void* node_data, void* data) {
////    const size_t *ID = (const size_t*)node_data;
////    *((size_t*)data) = *ID;
////}
////
////int pec_register_proxy(pec_store_t* dst, const char *file) {
////    size_t ID = 0;
////    if (pec_store_map_search(dst->files, file, pec_register_proxy_search, &ID) < 0)
////        return -1;
////    return pec_store_map_insert(dst->file_to_proxy, &ID, NULL);
////}
////
////pec_service_data_t* pec_get_service_data(pec_store_t* dst, size_t pid) {
////    return (pec_service_data_t*) pec_store_map_search(dst->service_to_file_id, &pid);
////}
////
