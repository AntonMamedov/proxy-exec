#ifndef STORE_H
#define STORE_H


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/rhashtable.h>
#include <linux/list.h>
#include <linux/fs.h>
#include "pec_buffer.h"

typedef struct files_node {
    struct filename* filename;
    uint32_t ID;            //ID файла
    int64_t service_PID;    //PID сервиса
    struct rhash_head head;
} files_node_t;

typedef struct proxy_node {
    uint32_t PID;
    uint32_t service_PID;
    struct pec_ring_buffer stdin;
    struct pec_ring_buffer stdout;
    struct task_struct* task;
    struct rhash_head head;
} proxy_node_t;

typedef struct service_node {
    uint32_t PID;
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
ssize_t pec_register_file(pec_store_t* dst, struct filename *filename);
int64_t pec_check_file(pec_store_t* src, struct filename *filename);
ssize_t pec_register_service(pec_store_t* dst, uint32_t PID, struct filename* filename);
bool pec_check_service(pec_store_t* src, uint32_t PID);
ssize_t pec_register_proxy(pec_store_t* dst, uint32_t PID, struct filename* filename);
size_t pec_delete_file(pec_store_t* dst, struct filename *filename);


#endif //STORE_H
