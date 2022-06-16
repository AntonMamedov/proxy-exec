#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/binfmts.h>

#include "callsyms.h"
#include "page_rw.h"
#include "logger.h"
#include "program_args.h"
#include "store.h"
#include "pec_buffer.h"

typedef enum pec_call pec_call_t;
typedef int (*syscall_t)(struct pt_regs*);

int pec_execve(struct pt_regs *args);

int pec_open(struct inode *, struct file *);
ssize_t pec_ioctl(struct file *, unsigned int, unsigned long);
ssize_t pec_read (struct file *, char __user *, size_t size, loff_t *);
ssize_t pec_write (struct file *, const char __user *, size_t, loff_t *);

static pec_store_t store;

enum entity_type {
    UNDEFINE = 0,
    SERVICE,
    SERVICE_WORKER,
    PROXY
};

struct pec_service_data {
    service_node_t *service_node;
};

struct pec_service_worker_data {
    service_node_t *service_node;
    proxy_node_t* proxy_node;
};

struct pec_proxy_data {
    service_node_t *service_node;
    proxy_node_t* proxy_node;
};

static struct{
    struct filename* (*getname)(const char*);
    void (*putname)(struct filename *name);
    syscall_t original_execve;
    syscall_t *syscall_table;
    int (*kernel_execve)(const char *, const char *const *, const char *const *);
} pec_meta;

static int pec_init_symbols(void) {
    pec_meta.getname = get_callsym_by_name("getname");
    if (pec_meta.getname == NULL) {
        FATAL("symbol 'getname' not found\n");
        return -1;
    }
    INFO("symbol 'getname' found\n");

    pec_meta.putname = get_callsym_by_name("putname");
    if (pec_meta.putname == NULL) {
        FATAL("symbol 'putname' not found\n");
        return -1;
    }
    INFO("symbol 'putname' found\n");

    pec_meta.kernel_execve = get_callsym_by_name("kernel_execve");
    if (pec_meta.kernel_execve == NULL) {
        FATAL("symbol 'kernel_execve' not found\n");
        return -1;
    }
    INFO("symbol 'kernel_execve' found\n");

    pec_meta.syscall_table = get_callsym_by_name("sys_call_table");
    if (pec_meta.syscall_table == NULL) {
        FATAL("symbol 'syscall_table' not found\n");
        return -1;
    }
    INFO("symbol 'syscall_table' found\n");

    enable_page_rw(pec_meta.syscall_table);
    pec_meta.original_execve = pec_meta.syscall_table[__NR_execve];
    pec_meta.syscall_table[__NR_execve] = pec_execve;
    disable_page_rw(pec_meta.syscall_table);
    if (pec_meta.original_execve == NULL) {
        FATAL("symbol 'execve' not found in syscall_table\n");
        return -1;
    }
    pec_store_init(&store);
    return 0;
}


static void pec_destroy_symbols(void) {
    enable_page_rw(pec_meta.syscall_table);
    pec_meta.syscall_table[__NR_execve] = pec_meta.original_execve;
    disable_page_rw(pec_meta.syscall_table);
    memset(&pec_meta, 0, sizeof(pec_meta));
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = pec_open,
    .unlocked_ioctl = pec_ioctl,
    .read = pec_read,
    .write = pec_write
};

static struct {
    dev_t dev;
    struct cdev pec_cdev;
    struct class *dev_class;
} device = {.dev = 0};

static int __init pec_init(void) {
    callsym_getter_init();

    if (alloc_chrdev_region(&device.dev, 0, 1, modname) < 0)
        FATAL("cannot allocate major\n");

    cdev_init(&device.pec_cdev, &fops);
    if (cdev_add(&device.pec_cdev, device.dev, 1) < 0) {
        unregister_chrdev_region(device.dev, 1);
        FATAL("cannot add the device to the system\n");
    }
    struct file *file;

    if ((device.dev_class = class_create(THIS_MODULE, modname)) == NULL) {
        unregister_chrdev_region(device.dev, 1);
        class_destroy(device.dev_class);
        FATAL("cannot create the struct class\n");
    }

    if((device_create(device.dev_class, NULL,device.dev, NULL, modname)) == NULL)
        FATAL("cannot create the PEC device \n\n");
    if (pec_init_symbols() < 0) {
        return 0;
    }
    INFO("device load success\n");
    return 0;
}

static void __exit pec_exit(void)
{
    pec_destroy_symbols();
    device_destroy(device.dev_class, device.dev);
    class_destroy(device.dev_class);
    cdev_del(&device.pec_cdev);
    unregister_chrdev_region(device.dev, 1);
    INFO("device unload success\n");
}


module_init(pec_init);
module_exit(pec_exit);


enum pec_call {
    REGISTER_FILE = 0,
    INIT_SERVICE = 1,
    REGISTER_SERVICE = 2,
    INIT_PROXY = 3,
};

struct register_service_data {
    const char* file;
    uint64_t service_id;
};

ssize_t register_file(struct file *file, const char* __user arg);
ssize_t init_service(struct file *file);
ssize_t register_service(struct file *file,  const char __user* arg);
ssize_t init_proxy(struct file *file,  uint64_t proxy_ID);

#define PATH_TO_PROXY ""

int pec_execve(struct pt_regs *args) {
    struct filename* fln = pec_meta.getname((const char*)args->di);
    service_node_t * pn = NULL;
    uint64_t service_id = 0;
    enum pec_store_error err = pec_store_get_service_by_file(&store, fln, &pn);
    pec_meta.putname(fln);
    if (err != OK) {
        return pec_meta.original_execve(args);
    }
    service_id = pn->ID;
    uint64_t proxy_id = 0;
    program_args_t* pg = new_program_args((const char*)args->di, (const char* const*) args->si, (const char* const*)args->dx, pec_meta.getname, pec_meta.putname);
    err = pec_store_create_proxy(&store, service_id, pg, &proxy_id);
    if (err != OK) {
        destroy_program_args(pg, pec_meta.putname);
        return -1;
    }
    INFO("a request was made to execute the file %s, an id=%llu was assigned to the proxy process", pg->file, proxy_id);
    char c_str_number[15];
    memset(c_str_number, 0, 15);
    sprintf(c_str_number, "%llu", proxy_id);
    const char* execve_kernel_args[] = {c_str_number, NULL};
    const char* execve_kernel_envs[] = {NULL};
    return pec_meta.kernel_execve(PATH_TO_PROXY, execve_kernel_args, execve_kernel_envs);
}

int pec_open(struct inode *inode, struct file *file) {
    return 0;
}

ssize_t pec_read (struct file *file, char __user *str, size_t size, loff_t *) {
    enum entity_type type = file->f_mode;
    struct pec_ring_buffer* r_buff = NULL;
    wait_queue_head_t * wait = NULL;
    spinlock_t* lock = NULL;
    switch (type) {
        case UNDEFINE:
            return -1;
        case SERVICE:
            return -1;
        case SERVICE_WORKER: {
            struct pec_service_worker_data * data = (struct pec_service_worker_data *) file->private_data;
            r_buff = &data->proxy_node->stdin;
            wait = &data->proxy_node->service_read_tasks;
            lock = &data->proxy_node->stdin_lock;
            break;
        }
        case PROXY: {
            struct pec_proxy_data * data = (struct pec_proxy_data *) file->private_data;
            r_buff = &data->proxy_node->stdout;
            wait = &data->proxy_node->proxy_read_tasks;
            lock = &data->proxy_node->stdout_lock;
        }
            break;
    }
    wait_event_interruptible(*wait, (r_buff->payload_len > 0));
    spin_lock(lock);
    ssize_t bytes_read = pec_ring_buffer_read(r_buff, str, size);
    spin_unlock(lock);
    return bytes_read;
    file->f_lock
}

ssize_t pec_write (struct file *file, const char __user *str, size_t size, loff_t *) {
    enum entity_type type = file->f_mode;
    struct pec_ring_buffer* r_buff = NULL;
    wait_queue_head_t * wait = NULL;
    spinlock_t* lock = NULL;

    switch (type) {
        case UNDEFINE:
            return -1;
        case SERVICE:
            return -1;
        case SERVICE_WORKER: {
            struct pec_service_worker_data * data = (struct pec_service_worker_data *) file->private_data;
            r_buff = &data->proxy_node->stdout;
            wait = &data->proxy_node->proxy_read_tasks;
            lock = &data->proxy_node->stdout_lock;
        }
            break;
        case PROXY: {
            struct pec_proxy_data * data = (struct pec_proxy_data *) file->private_data;
            r_buff = &data->proxy_node->stdin;
            wait = &data->proxy_node->service_read_tasks;
            lock = &data->proxy_node->stdin_lock;
        }
            break;
    }
    spin_lock(lock);
    pec_ring_buffer_write(r_buff, str, size);
    spin_unlock(lock);
    wake_up(wait);
}

ssize_t pec_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    if (cmd > 7)
        return -EPERM;

    enum pec_call call = cmd;
    switch (call) {
        case REGISTER_FILE:
            return register_file(file, arg);
        case REGISTER_SERVICE:
            return register_service(file, arg);
        case INIT_SERVICE:
            return init_service(file);
        case INIT_PROXY:
            return init_proxy(file, arg);
    }
}

ssize_t register_file(struct file *file, const char __user*  arg) {
    struct filename* f = pec_meta.getname(arg);
    if (pec_store_register_file(&store, f) == FILE_ALREADY_EXISTS) {
        return -EBUSY;
    }
    return 0;
}

ssize_t init_service(struct file *file) {
    uint64_t service_id = 0;
    service_node_t * sn = NULL;
    enum pec_store_error err = pec_store_create_service(&store, &sn);
    if (err != OK) {
        return -EPERM;
    }
    struct pec_service_data* d = vmalloc(sizeof(struct pec_service_data*));
    file->private_data = d;
    return sn->ID;
}

ssize_t register_service(struct file *file,  const char __user* arg) {
    struct filename* fname = pec_meta.getname(arg);
    uint64_t service_id = ((struct pec_service_data*)file->private_data)->service_node->ID;
    uint64_t file_id = 0;
    enum pec_store_error err = pec_store_associate_service_with_file(&store, service_id, fname, &file_id);
    vfree(fname);
    switch (err) {
        case OK:
            return file_id;
        default:
            break;
    }
    return -EPERM;
}

ssize_t init_proxy(struct file *file,  uint64_t proxy_ID) {
    proxy_node_t* n = NULL;
    pec_store_get_proxy_data(&store, proxy_ID, &n);
    if (n == NULL) {
        return -1;
    }
    struct pec_proxy_data* data = vmalloc(sizeof(struct pec_proxy_data*));
    data->proxy_node = n;
    data->service_node = get_service_node_by_id(&store, n->service_ID);
    file->private_data = data;
    return 0;
}

ssize_t register_service_worker(struct file *file,  uint64_t proxy_ID) {
    proxy_node_t* n = NULL;
    pec_store_get_proxy_data(&store, proxy_ID, &n);
    if (n == NULL) {
        return -1;
    }
    struct pec_proxy_data* data = vmalloc(sizeof(struct pec_proxy_data*));
    data->proxy_node = n;
    data->service_node = get_service_node_by_id(&store, n->service_ID);
    file->private_data = data;
    return 0;
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.1");
MODULE_AUTHOR("Mamedov Anton");

