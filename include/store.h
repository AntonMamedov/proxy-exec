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
    pec_buffer stdin;
    pec_buffer stdout;
    struct rhash_head head;
    struct list_head list;
}proxy_node_t;

typedef struct services_node {
    uint32_t PID;
    struct list_head proxy_list;
    struct rhash_head head;
} services_node_t;

typedef struct pec_store {
    struct rhashtable files;
    struct rhashtable services;
    struct rhashtable proxy;
    spinlock_t lock;
} pec_store_t;

int pec_store_init(pec_store_t* dst);
int pec_store_destroy(pec_store_t* dst);
ssize_t pec_register_file(pec_store_t* dst, struct filename *filename);
int64_t pec_check_file(pec_store_t* src, struct filename *filename);

#endif //STORE_H
