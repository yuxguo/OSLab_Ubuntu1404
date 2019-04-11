#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <linux/kernel.h>
#include <stdarg.h>

int main (void)
{
	char str[100];
	int ret;
	printf("Give me a string:\n");
	scanf("%s",str);
	int len = strlen(str);
	syscall(328,&str,len+1,&ret);
	syscall(327,ret);
	return 0;
	
}
