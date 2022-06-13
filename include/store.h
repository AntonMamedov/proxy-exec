#ifndef STORE_H
#define STORE_H


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/rhashtable.h>

typedef struct files_node {
    uint32_t ID;
    int64_t service_PID;
    struct rhash_head head;
} files_node_t;

typedef struct pec_store {
    struct rhashtable files;
} pec_store_t;

int pec_store_init(pec_store_t* dst);
int pec_store_destroy(pec_store_t* dst);
ssize_t pec_register_file(pec_store_t* dst, uint32_t ID);

#endif //STORE_H
