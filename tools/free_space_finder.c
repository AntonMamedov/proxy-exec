#include <linux/module.h>

#include "callsyms.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.01");

static void **taddr, *niaddr;           // адрес sys_call_table  // адрес sys_ni_syscall()
static int nsys = 0;                    // число системных вызовов в версии

typedef int (*syscall_wrapper)(struct pt_regs *);

#define SYS_NR_MAX 450                 // SYS_NR_MAX - произвольно большое, больше длины sys_call_table

static int sys_length( void* data, const char* sym, struct module* mod, unsigned long addr ) {
    int i;
    if( ( strstr( sym, "sys_" ) != sym ) ||
        ( 0 == strcmp( "sys_call_table", sym ) ) ) return 0;
    for( i = 0; i < SYS_NR_MAX; i++ ) {
        if( taddr[ i ] == (void*)addr ) {  // найден sys_* в sys_call_table
            if( i > nsys ) nsys = i;
            break;
        }
    }
    return 0;
}

static void put_entries( void ) {
    int i, ni = 0;
    char buf[ 200 ] = "";
    for( i = 0; i <= nsys; i++ )
        if( taddr[ i ] == niaddr ) {
            ni++;
            sprintf( buf + strlen( buf ), "%03d, ", i );
        }
    pr_info( "found %d unused entries: %s\n", ni, buf );
}

typedef int (*kallsyms_on_each_symbol_p)(int (*fn)(void *, const char *, struct module *, unsigned long), void *);

static int __init fsf_init(void) {
    callsym_getter_init();
    kallsyms_on_each_symbol_p koes = (kallsyms_on_each_symbol_p) get_callsym_by_name("kallsyms_on_each_symbol");

    if (koes == NULL)
        pr_err("kallsyms_on_each_symbol not found\n");

    if( NULL == ( taddr = (void**) get_callsym_by_name("sys_call_table") ) ) {
        pr_info( "sys_call_table not found!\n" );
        return -EFAULT;
    }

    if( NULL == ( niaddr = (void*) get_callsym_by_name("sys_ni_syscall") ) ) {
        pr_info( "sys_ni_syscall found!\n" );
        return -EFAULT;
    }

    pr_info("sys_ni_syscall address = %p\n", niaddr );

    syscall_wrapper* table = (syscall_wrapper*)taddr;
    size_t i;
    for (i = 0; i < 500; i++) {
        if (table[i] == (syscall_wrapper)niaddr) {
            pr_info("empty cell = %lu\n", i );
        }
    }
    return -EPERM;
}

static void __exit fsf_exit(void) {}

module_init( fsf_init )
module_exit(fsf_exit)