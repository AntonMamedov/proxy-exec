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

typedef enum pec_call pec_call_t;
typedef int (*syscall_t)(struct pt_regs*);

int pec_execve(struct pt_regs *args);
int pec_open(struct inode *inode, struct file *file);
ssize_t pec_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t pec_read (struct file *f, char __user *d, size_t size, loff_t *);
ssize_t pec_write (struct file *, const char __user *, size_t, loff_t *);

static pec_store_t store;

enum entity_type{
    PROXY = 1 << 0,
    SERVICE = 1 << 1,
    REGISTRAR = 1 << 2
};

struct pec_data {
    enum entity_type type;
    struct task_struct service_task;
    uint32_t service_PID;
    uint32_t proxy_PID;
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
    REGISTER_FILE          = 0,
    REGISTER_PROXY_PROCESS = 1,
    REGISTER_PROXY_SERVICE = 2,
    SET_PROGRAM_ARGS       = 3,
    SET_ENVS               = 4,
    WRITE_STDOUT            = 6,
    READ_STDIN            = 7,
};


ssize_t set_program_args(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t register_file(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t register_proxy_process(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t register_service_process(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t wrute_stdout(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t set_envs(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t read_stdin(struct file *file, unsigned int cmd, unsigned long arg);

ssize_t pec_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    if (cmd > 7)
        return -EPERM;
    pec_call_t call = (pec_call_t)cmd;
    switch (call) {
        case REGISTER_FILE:
            register_file(file, cmd, arg);
            break;
        case REGISTER_PROXY_PROCESS:
            register_proxy_process(file, cmd, arg);
            break;
        case REGISTER_PROXY_SERVICE:
            register_service_process(file, cmd, arg);
            break;
        case SET_PROGRAM_ARGS:
            return set_program_args(file, cmd, arg);
        case SET_ENVS:
            set_envs(file, cmd, arg);
            break;
        case WRITE_STDOUT:
            wrute_stdout(file, cmd, arg);
            break;
        case READ_STDIN:
            read_stdin(file, cmd, arg);
            break;
    }

    return 0;
}


int pec_execve(struct pt_regs *args) {
    struct filename* fln = pec_meta.getname((const char*)args->di);

    new_program_args((const char*)args->di, (const char* const*) args->si, (const char* const*)args->dx, pec_meta.getname, pec_meta.putname);
    return pec_meta.original_execve(args);
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.1");
MODULE_AUTHOR("Mamedov Anton");