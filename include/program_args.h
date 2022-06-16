#ifndef LINUX_KERNEL_MODULE_WITH_CLION_IDE_SUPPORT_CMAKE_PROGRAM_ARGS_H
#define LINUX_KERNEL_MODULE_WITH_CLION_IDE_SUPPORT_CMAKE_PROGRAM_ARGS_H
#include <linux/kernel.h>
#include <linux/fs.h>
typedef struct program_args {
    char* file;
    char** arg;
    char** envp;
} program_args_t;

program_args_t* new_program_args(const char __user *path, const char __user *const args[], const char __user *const *envp,
                                 struct filename* (*getname)(const char*), void (*putname)(struct filename *name));
void destroy_program_args(program_args_t *dst, void (*putname)(struct filename *name));
#endif //LINUX_KERNEL_MODULE_WITH_CLION_IDE_SUPPORT_CMAKE_PROGRAM_ARGS_H
