#ifndef STORE_H
#define STORE_H

#define PEC_STORE_KERNEL

#ifdef PEC_STORE_KERNEL
#include <linux/kernel.h>
#else
#include "stdlib.h"
#endif

struct pec_store_map;
typedef struct pec_store_map pec_store_map_t;


struct pec_store {
    pec_store_map_t* file_to_file_id;     // file path -> file ID
    pec_store_map_t* service_to_file_id;  // service PID -> file ID
    pec_store_map_t* file_to_proxy;       // file ID -> {proxy PID, ...}
    pec_store_map_t* proxy_to_service;    // proxy PID -> service PID
};

typedef struct pec_store pec_store_t;

struct proxy_pid_node {
    size_t PID;                  // proxy PID
    struct proxy_pid_node* next; // next node
};

typedef struct proxy_pid_node proxy_pid_node_t;

struct pec_service_data {
    size_t PID;              // file PID
    proxy_pid_node_t* head;  // head proxy pid list
};

typedef struct pec_service_data pec_service_data_t;

int pec_store_init(pec_store_t* dst);
//int pec_set_filepath_and_generate_file_id(pec_store_t* dst, const unsigned char* file);
//int pec_register_service(pec_store_t* dst, size_t pid, size_t ID);
//int pec_register_proxy(pec_store_t* dst, const char* file);
//pec_service_data_t* pec_get_service_data(pec_store_t* dst, size_t pid);
#endif //STORE_H
