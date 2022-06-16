#ifndef STORE_H
#define STORE_H


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/rhashtable.h>
#include <linux/list.h>
#include <linux/fs.h>

#include "pec_buffer.h"
#include "program_args.h"

enum pec_store_error {
    OK = 0,
    FILE_NOT_FOUND,
    FILE_ALREADY_EXISTS,
    SERVICE_NOT_REGISTERED,
    FILE_ALREDY_ACCOCIATED,
    UNABLE_TO_REGISTER_THE_SERVICE,
    UNABLE_TO_REGISTER_THE_PROXY,
    BAD_ARG
};

typedef struct files_node {
    struct filename* filename;
    uint64_t file_ID;
    uint64_t service_ID;
    bool service_registered;
    struct rhash_head head;
} files_node_t;

typedef struct proxy_node {
    uint64_t ID;
    uint64_t service_ID;
    struct pec_ring_buffer stdin;
    struct pec_ring_buffer stdout;
    program_args_t* pa;
    wait_queue_head_t proxy_read_tasks;
    wait_queue_head_t proxy_get_recode_tasks;
    wait_queue_head_t service_read_tasks;
    spinlock_t stdin_lock;
    spinlock_t stdout_lock;
    struct rhash_head head;
} proxy_node_t;

typedef struct service_node {
    uint32_t ID;
    wait_queue_head_t service_get_proxy_task;
    struct rhash_head head;
} service_node_t;

typedef struct pec_store {
    struct rhashtable files;
    struct rhashtable proxy;
    struct rhashtable services;
    spinlock_t file_lock;
    spinlock_t proxy_lock;
    spinlock_t service_lock;
} pec_store_t;

int pec_store_init(pec_store_t* dst);

int pec_store_destroy(pec_store_t* dst);
enum pec_store_error pec_store_register_file(pec_store_t* dst, struct filename *filename);
enum pec_store_error pec_store_create_service(pec_store_t* dst, service_node_t**);
enum pec_store_error pec_store_associate_service_with_file(pec_store_t* dst, uint64_t service_ID, struct filename* filename, uint64_t *fileId);
enum pec_store_error pec_store_get_service_by_file(pec_store_t* dst, struct filename* filename, service_node_t**);
enum pec_store_error pec_store_create_proxy(pec_store_t* dst, uint64_t service_ID, program_args_t* pa, uint64_t* proxy_id);
enum pec_store_error pec_store_get_proxy_data(pec_store_t* dst, uint64_t proxy_ID, proxy_node_t** proxy_data);
service_node_t* get_service_node_by_id(pec_store_t* dst, uint64_t service_id);
#endif //STORE_H
