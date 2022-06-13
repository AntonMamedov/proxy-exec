#include "program_args.h"

#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/binfmts.h>

#include "logger.h"

// from fs/exec.c
struct user_arg_ptr {
#ifdef CONFIG_COMPAT
    bool is_compat;
#endif
    union {
        const char __user *const __user *native;
#ifdef CONFIG_COMPAT
        const compat_uptr_t __user *compat;
#endif
    } ptr;
};

static int count(struct user_arg_ptr argv, int max);

program_args_t* new_program_args(const char __user *path, const char __user *const args[], const char __user *const *envp,
                                 struct filename* (*getname)(const char*), void (*putname)(struct filename *name)) {
    program_args_t* pr_args = vzalloc(sizeof(program_args_t));
    if (pr_args == NULL)
        return NULL;

    pr_args->file = getname(path);
    if (pr_args == NULL)
        goto OUT_FREE;

    struct user_arg_ptr argv = { .ptr.native = args };
    struct user_arg_ptr envs = { .ptr.native = envp };
    int retval = count(argv, MAX_ARG_STRINGS);
    INFO("filename=%s, argv_count=%d", pr_args->file->name, retval);
    if (retval < 0)
        goto OUT_FREE;

    pr_args->arg = vzalloc(sizeof(char*) * retval + 1);
    if (copy_from_user(pr_args->arg, args, sizeof(char*) * retval) < 0) {
        ERROR("copy argv error\n");
        goto OUT_FREE;
    }
    INFO("copy argv success\n");
    size_t i;
    for (i = 0; i < retval; i++) {
        const char* tmp = pr_args->arg[i];
        ssize_t strlen = strnlen_user(tmp, MAX_ARG_STRLEN);
        if (strlen == 0)
            break;
        if (strlen < 0) {
            ERROR("bad strlen\n")
            goto OUT_FREE;
        }
        pr_args->arg[i] = vzalloc(sizeof(char) * strlen + 1);
        if (copy_from_user(pr_args->arg[i], tmp, sizeof(char) * strlen) < 0) {
            ERROR("copy argv element error\n");
            goto OUT_FREE;
        }
        if (pr_args->arg[i] != NULL)
            INFO("file=%s argv[%lu] = %s\n",pr_args->file->name,i, pr_args->arg[i]);
    }

    retval = count(envs, MAX_ARG_STRINGS);
    INFO("filename=%s, env_count=%d", pr_args->file->name, retval);
    if (retval < 0)
        goto OUT_FREE;
    pr_args->envp = vzalloc(sizeof(char*) * retval + 1);
    if (copy_from_user(pr_args->envp, args, sizeof(char*) * retval) < 0) {
        ERROR("copy envp error\n");
        goto OUT_FREE;
    }
    INFO("copy envp success\n");
    for (i = 0; i < retval; i++) {
        const char* tmp = pr_args->envp[i];
        ssize_t strlen = strnlen_user(tmp, MAX_ARG_STRLEN);
        if (strlen == 0)
            break;
        if (strlen < 0) {
            ERROR("bad strlen\n")
            goto OUT_FREE;
        }
        pr_args->envp[i] = vzalloc(sizeof(char) * strlen + 1);
        if (copy_from_user(pr_args->envp[i], tmp, sizeof(char) * strlen) < 0) {
            ERROR("copy envp element error\n");
            goto OUT_FREE;
        }
        if (pr_args->envp[i] != NULL)
            INFO("file=%s envp[%lu] = %s\n",pr_args->file->name, i, pr_args->envp[i]);
    }
    return pr_args;
    OUT_FREE:
    destroy_program_args(pr_args, putname);
    return NULL;
}

void destroy_program_args(program_args_t *dst, void (*putname)(struct filename *name)) {
    if (dst == NULL)
        return;
    if (dst->file != NULL)
        putname(dst->file);
    if (dst->arg != NULL) {
        size_t i;
        for (i = 0; dst->arg[i] != NULL; i++)
            vfree(dst->arg);
        vfree(dst->arg);
    }
    if (dst->envp != NULL) {
        size_t i;
        for (i = 0; dst->envp[i] != NULL; i++)
            vfree(dst->envp);
        vfree(dst->envp);
    }

    vfree(dst);
}

// from fs/exec.c
static const char __user *get_user_arg_ptr(struct user_arg_ptr argv, int nr)
{
    const char __user *native;

#ifdef CONFIG_COMPAT
    if (unlikely(argv.is_compat)) {
        compat_uptr_t compat;

        if (get_user(compat, argv.ptr.compat + nr))
            return ERR_PTR(-EFAULT);

        return compat_ptr(compat);
    }
#endif

    if (get_user(native, argv.ptr.native + nr))
        return ERR_PTR(-EFAULT);

    return native;
}

static bool valid_arg_len(long len)
{
    return len <= MAX_ARG_STRLEN;
}


/*
 * count() counts the number of strings in array ARGV.
 */
int count(struct user_arg_ptr argv, int max)
{
    int i = 0;

    if (argv.ptr.native != NULL) {
        for (;;) {
            const char __user *p = get_user_arg_ptr(argv, i);

            if (p == NULL)
                break;

            if (IS_ERR(p))
                return -EFAULT;

            if (i >= max)
                return -E2BIG;
            ++i;

            if (fatal_signal_pending(current))
                return -ERESTARTNOHAND;
            cond_resched();
        }
    }
    return i;
}