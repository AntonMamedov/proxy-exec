#ifndef STORE_H
#define STORE_H
#define PEC_STORE_TEST_BUILD

#ifndef PEC_STORE_TEST_BUILD
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#else
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#endif


typedef struct pec_list_node {
    struct pec_list_node* next;
    void* data;
}pec_list_node_t;

typedef struct pec_list {
    pec_list_node_t* head;
    pec_list_node_t* tail;
    size_t size;
} pec_list_t;

size_t default_string_hash(const void* str);

int pec_list_push_back_with_duplicate_check(pec_list_t* dst, void* data, int (*cmp)(const void*, const void*), const void* (*get)(const void*));
bool pec_list_delete_first(pec_list_t* dst, void* data, bool(*fn)(void*, void*));
void pec_list_destroy(pec_list_t* dst, void (*fn)(void*));
pec_list_node_t* pec_list_pop_front_node(pec_list_t* dst);
void pec_list_push_back_node(pec_list_t* dst, pec_list_node_t* node);

struct pec_hashtable;

typedef struct pec_hashtable_operation {
    size_t (*hash)(const void* /* key */);
    const void* (*get_key)(const void* /*node data */);
    int (*key_cmp)(const void* /*l key*/, const void* /*r key*/);
    void (*on_extend_start)(struct pec_hashtable* /* old table */, void*);
    void (*on_extend_end)(struct pec_hashtable* /* new table */, void*);
    void (*on_insert_start)(struct pec_hashtable* /*table */, size_t /*index*/, void* /* external data*/);
    void (*on_insert_end)(struct pec_hashtable* /*table */, size_t /*index*/, void* /* external data*/);
    void (*on_read_start)(struct pec_hashtable* /*table */, size_t /*index*/, void* /* external data*/);
    void (*on_read_end)(struct pec_hashtable* /*table */, size_t /*index*/, void* /* external data*/);
    void (*on_node_destroy)(void* /* node data */);
} pec_hashtable_operation_t;


typedef struct pec_hashtable {
    pec_list_t* buckets;
    size_t size;
    size_t load;
    const pec_hashtable_operation_t* ops;
    uint8_t load_factor_major;
    uint8_t load_factor_minor;
} pec_hashtable_t;

#define PEC_ERROR_MEM_ALLOC 1
#define PEC_BAD_ARG 2
#define PEC_DUPLICATE 3

int pec_hashtable_init(pec_hashtable_t* dst, size_t size,
                       size_t load_factor_major, size_t load_factor_minor, const pec_hashtable_operation_t* ops);
int pec_hashtable_destroy(pec_hashtable_t* dst);
int pec_hashtable_inset(pec_hashtable_t* dst, void* data, void* external_data);
int pec_hashtable_search(const pec_hashtable_t *src, void* key, void* external_data, void (*fn)(void*, void*));
#endif //STORE_H
