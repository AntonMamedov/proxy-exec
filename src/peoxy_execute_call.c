#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include "callsyms.h"
#include "page_rw.h"
#include <asm/uaccess.h>
#include "store.h"
#include "linux/vmalloc.h"
#include "linux/binfmts.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.1");

typedef int (*syscall_wrapper)(struct pt_regs *);

syscall_wrapper original_execve = NULL;
static void* syscall_table_addr = NULL;


typedef enum call{
    REGISTER_FILE          = 0,
    REGISTER_PROXY_PROCESS = 1,
    REGISTER_PROXY_SERVICE = 2,
    SET_PROGRAM_ARGS       = 3,
    SET_ENVS               = 4,
    SET_SIGNAL             = 5,
    WRITE_STDIN            = 6,
    READ_STDOUT            = 7,
} call_t;

/* structs for proxy-exec calls args */

struct program_argument_arg {
    int argc;
    char** argv;
};

struct register_file_arg {
    int len;
    char* path;
};

struct register_proxy_arg {

};


static int pec_open(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t register_file(struct file *file, struct register_file_arg* arg) {
//    struct register_file_arg file_data;
//    if (copy_from_user(&file_data, arg, sizeof(struct register_file_arg)) < 0)
//        return -EINVAL;
//    file_data.path = kmalloc(file_data.len * sizeof(char), GFP_KERNEL);
//    if (copy_from_user(&file_data.path, arg->path, file_data.len * sizeof(char)) < 0)
//        return -EINVAL;
//
//    if (pec_set_filepath_and_generate_file_id(&pec_store, (uint8_t*)file_data.path) < 0) {
//        kfree(file_data.path);
//        return -EBUSY;
//    }
//
//    return 0;
    return 0;
}

static ssize_t set_program_args(struct file *file, unsigned int cmd, unsigned long arg) {
    return 0;
}

static ssize_t pec_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    if (cmd > 7)
        return -EPERM;
    call_t call = (call_t)cmd;
    switch (call) {
        case REGISTER_FILE:
            return register_file(file, (struct register_file_arg*)arg);
        case REGISTER_PROXY_PROCESS:
            break;
        case REGISTER_PROXY_SERVICE:
            break;
        case SET_PROGRAM_ARGS:
            return set_program_args(file, cmd, arg);
        case SET_ENVS:
            break;
        case SET_SIGNAL:
            break;
        case WRITE_STDIN:
            break;
        case READ_STDOUT:
            break;
    }

    return 0;
//    pr_info("Pec ioctl called\n");
//    struct program_argument_arg pga;
//    if (copy_from_user(&pga, (struct program_argument_arg*)arg, sizeof(struct program_argument_arg))) {
//        pr_err("PEC error cpy from user\n");
//        return -1;
//    }
//    char* argggg[3];
//    if (copy_from_user(argggg, pga.argv, sizeof(char*) * 3) < 0) {
//        pr_err("PEC error cpy from user\n");
//        return -1;
//    }
//    pr_info("Pec ioctl arg: %p\n", argggg);
//
//    size_t i;
//    for (i = 0; i < pga.argc; i++) {
//        char buffer[10];
//        if (copy_from_user(buffer, argggg[i], 3) < 0) {
//            pr_err("PEC error cpy from user\n");
//            return -1;
//        }
//        pr_err("PEC PGA - %s\n", buffer);
//    }
//    return 0;
}

int (*exec_wrapper)(const char *filename,
                    const char *const *argv, const char *const *envp) = NULL;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = pec_open,
    .unlocked_ioctl = pec_ioctl
};

dev_t dev = 0;
static struct cdev pec_cdev;
static struct class *dev_class;

const char* test_path = "/home/amamedov/dev/university/proxy-exec/tests/test";
char* __user test_proxy_path2 = NULL;
const char* test_proxy_path = "/home/amamedov/dev/university/proxy-exec/tests/build/dev_test";
const char* const argv[] = {"1", "2", NULL};
const char* const env[] = {NULL};
int pec_execve(struct pt_regs *args) {
    char buffer[256];
    raw_copy_from_user(buffer, (const char*)args->di, sizeof(buffer));
    if (strcmp(buffer, test_path) == 0) {
        pr_info("pec_execve: file path[%s]", buffer);

        return exec_wrapper(test_proxy_path, argv, env);
    }
    return original_execve(args);
}

static int __init pec_init(void) {
    if (alloc_chrdev_region(&dev, 0, 1, "pec") < 0) {
        pr_err("Cannot allocate major number\n");
        return -1;
    }
    struct filename a;
    pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));
    cdev_init(&pec_cdev, &fops);
    pec_cdev.owner = THIS_MODULE;
    pec_cdev.ops = &fops;
    if (cdev_add(&pec_cdev, dev, 1) < 0) {
        pr_err("Cannot add PEC the device to the system\n");
        goto ERR_ADD_DEV;
    }

    if ((dev_class = class_create(THIS_MODULE, "pec_class")) == NULL) {
        pr_err("Cannot create the struct class\n");
        goto ERR_ADD_DEV;
    }

    /*Creating device*/
    if((device_create(dev_class,NULL,dev,NULL,"pec_device")) == NULL){
        pr_err("Cannot create the PEC Device 1\n");
        goto ERR_CREATE_DEV;
    }

    pr_info("Device PEC Driver Insert...Done!!!\n");
    callsym_getter_init();
    syscall_table_addr =  get_callsym_by_name("sys_call_table");
    enable_page_rw(syscall_table_addr);
    original_execve = ((syscall_wrapper*)syscall_table_addr)[__NR_execve];
    ((syscall_wrapper*)syscall_table_addr)[__NR_execve] = pec_execve;
    disable_page_rw(syscall_table_addr);
    test_proxy_path2 = vmalloc_user(sizeof(char) * (strlen(test_path) + 1));
    strcpy(test_proxy_path2, test_proxy_path);
    test_proxy_path2[strlen(test_proxy_path)] = '\0';
    exec_wrapper = get_callsym_by_name("kernel_execve");
    if (exec_wrapper == NULL) {
        pr_err("execve not found\n");
        return -1;
    }
    return 0;
    ERR_CREATE_DEV:
    class_destroy(dev_class);

    ERR_ADD_DEV:
    unregister_chrdev_region(dev, 1);

    return -1;
}

static void __exit pec_exit(void)
{
    vfree(test_proxy_path2);
    enable_page_rw(syscall_table_addr);
    ((syscall_wrapper*)syscall_table_addr)[__NR_execve] = original_execve;
    disable_page_rw(syscall_table_addr);
    device_destroy(dev_class,dev);
    class_destroy(dev_class);
    cdev_del(&pec_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Device Driver PEC Remove...Done!!!\n");
}


module_init(pec_init);
module_exit(pec_exit);