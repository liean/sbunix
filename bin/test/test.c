#include <stdio.h>
#include <syscall.h>

#define TEST "TEST\n"

int temp;

int main()
{
	pid_t pid;
	static volatile unsigned int d = 0xdeadbeef;
	int test;
	test = printf("%s", TEST);

	pid = fork();

	if (pid == 0) {
		*(uint64_t *)0x7fffffffeff0 = 0xdeadbeef;
		d = 1;
		printf("I'm child, pid=%d\n", getpid());
		test = execve("/bin/hello", NULL, NULL);
		printf("execve %d\n", test);
	} else {
		*(uint64_t *)0x7fffffffeff0 = 0xdeadbeef;
		d = 2;
		printf("I'm parent, child id=%d\n", pid);
	}

	printf("%d\n", test);

	while (d)
		printf("TTTTTEEEESSSSTTTT\n");

	printf("%s", TEST);
	return 0;
}

/* vim: set ts=4 sw=0 tw=0 noet : */
