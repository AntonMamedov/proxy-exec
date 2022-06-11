#include "callsyms.h"

#include <linux/kernel.h>
#include <linux/kprobes.h>

#define KPROBE_PRE_HANDLER(fname) static int __kprobes fname(struct kprobe *p, struct pt_regs *regs)
typedef unsigned long (*kallsyms_lookup_name_p)(const char* kallsyms_name);

static struct kprobe kp0, kp1;
static uint64_t  kallsyms_lookup_name_addr = 0;
static kallsyms_lookup_name_p kallsyms_lookup_name_func = NULL;

KPROBE_PRE_HANDLER(handler_pre0) {
    kallsyms_lookup_name_addr = (--regs->ip);
    return 0;
}

KPROBE_PRE_HANDLER(handler_pre1) {
    return 0;
}

static int do_register_kprobe(struct kprobe* kp, char* symbol_name, void* handler) {
    int ret;

    kp->symbol_name = symbol_name;
    kp->pre_handler = handler;

    ret = register_kprobe(kp);
    if (ret < 0) {
        pr_err("do_register_kprobe: failed to register for symbol %s, returning %d\n", symbol_name, ret);
        return ret;
    }

    pr_info("Planted krpobe for symbol %s at %p\n", symbol_name, kp->addr);

    return ret;
}

int callsym_getter_init(void) {
    if (kallsyms_lookup_name_func != NULL)
        return 0;

    int status;
    status = do_register_kprobe(&kp0, "kallsyms_lookup_name", handler_pre0);

    if (status < 0) return status;

    status = do_register_kprobe(&kp1, "kallsyms_lookup_name", handler_pre1);

    if (status < 0) {
        // cleaning initial krpobe
        unregister_kprobe(&kp0);
        return status;
    }

    unregister_kprobe(&kp0);
    unregister_kprobe(&kp1);

    pr_info("kallsyms_lookup_name address = 0x%llx\n", kallsyms_lookup_name_addr);
    kallsyms_lookup_name_func = (kallsyms_lookup_name_p)kallsyms_lookup_name_addr;
    return 0;
}

void* get_callsym_by_name(const char* callsym_name) {
    if (kallsyms_lookup_name_func == NULL) {
        return NULL;
    }

    return (void*)kallsyms_lookup_name_func(callsym_name);
}
