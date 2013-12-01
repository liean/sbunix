#ifndef _SYS_X86_H
#define _SYS_X86_H

#include <defs.h>

static inline uint64_t rcr2(void)
{
	uint64_t cr2;

	__asm__ volatile("movq %%cr2, %0" : "=r" (cr2));
	return cr2;
}
#endif
/* vim: set ts=4 sw=0 tw=0 noet : */
