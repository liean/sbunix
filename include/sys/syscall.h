#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <syscall_no.h>

int syscall_init(void);

void _syscall_lstar(void);

void _syscall_lstar_ret(void);

void _syscall_cstar(void);

#endif
/* vim: set ts=4 sw=0 tw=0 noet : */
