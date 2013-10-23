#include <defs.h>
#include <sys/gdt.h>
#include <sys/idt.h>

#include <console.h>
#include <printf.h>

/* FIXME: ENTER trick */
int volatile enter_pressed;

void start(uint32_t* modulep, void* physbase, void* physfree)
{
	int rval = 0;
	struct smap_t {
		uint64_t base, length;
		uint32_t type;
	}__attribute__((packed)) *smap;

	// kernel starts here
	rval = console_init();
	if (rval != 0) {
		/* FIXME:
		 * put error handler here
		 */
	}
	setup_idt();

	{
		rval = printf("Printf Test");
		printf(" (rval = \27[32m%d\27[m)\n", rval);
		rval = printf("\27[34m%%c\27[m: %c %c %c %c %c", 'a', 'B', 'c', ',', '0');
		printf(" (rval = \27[32m%d\27[m)\n", rval);
		rval = printf("\27[34m%%s\27[m: %s", "deadbeef");
		printf(" (rval = \27[32m%d\27[m)\n", rval);
		rval = printf("\27[34m%%d\27[m: %d", 1234567890);
		printf(" (rval = \27[32m%d\27[m)\n", rval);
		rval = printf("\27[34m%%p\27[m: %p", 0);
		printf(" (rval = \27[32m%d\27[m)\n", rval);
		rval = printf("\27[34m%%x\27[m: %x", 0xdeadbeef);
		printf(" (rval = \27[32m%d\27[m)\n", rval);

		printf("\nASCII Color Test\n");
		printf("\t\27[30m*[30m");
		printf("\27[31m*[31m");
		printf("\27[32m*[32m");
		printf("\27[33m*[33m");
		printf("\27[34m*[34m");
		printf("\27[35m*[35m");
		printf("\27[37m*[37m\n");

		printf("\t\27[40m*[40m");
		printf("\27[41m*[41m");
		printf("\27[42m*[42m");
		printf("\27[43m*[43m");
		printf("\27[44m*[44m");
		printf("\27[45m*[45m");
		printf("\27[47m*[47m\n");

		while (!enter_pressed)
			;

		enter_pressed = 0;
		printf("\n");
	}


	while(modulep[0] != 0x9001) modulep += modulep[1]+2;
	for(smap = (struct smap_t*)(modulep+2); smap < (struct smap_t*)((char*)modulep+modulep[1]+2*4); ++smap) {
		if (smap->type == 1 /* memory */ && smap->length != 0) {
			printf("Available Physical Memory [%x-%x]\n", smap->base, smap->base + smap->length);
		}
	}

	while (1) /* We are not expected to return */
		;
}

#define INITIAL_STACK_SIZE 4096
char stack[INITIAL_STACK_SIZE];
uint32_t* loader_stack;
extern char kernmem, physbase;

void boot(void)
{
	// note: function changes rsp, local stack variables can't be practically used
	register char *temp1, *temp2;
	__asm__(
		"movq %%rsp, %0;"
		"movq %1, %%rsp;"
		:"=g"(loader_stack)
		:"r"(&stack[INITIAL_STACK_SIZE])
	);
	reload_gdt();
	setup_tss();
	start(
		(uint32_t*)((char*)(uint64_t)loader_stack[3] + (uint64_t)&kernmem - (uint64_t)&physbase),
		&physbase,
		(void*)(uint64_t)loader_stack[4]
	);
	for(
		temp1 = "!!!!! start() returned !!!!!", temp2 = (char*)0xb8000;
		*temp1;
		temp1 += 1, temp2 += 2
	) *temp2 = *temp1;
	while(1);
}
