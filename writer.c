#include <stdio.h>
#include <stdlib.h>
#include "memmap.h"

#define _GNU_SOURCE
#include <sys/uio.h>

#include <errno.h>
#include <string.h>

#include <unistd.h>



void change_value(void *addr, pid_t pid, int new_value)
{
	struct iovec local[1];
	struct iovec remote[1];
	int          buf1;
	ssize_t      nread;

	int				needed_len = sizeof(int);

	buf1 = new_value;
	local[0].iov_base = (void *)&buf1;
	local[0].iov_len = needed_len;

	remote[0].iov_base = addr;
	remote[0].iov_len = needed_len;

	nread = process_vm_writev(pid, local, 1, remote, 1, 0);
	if (nread == -1)
	{
		fprintf(stderr, "Failed to write %d: %s\n", pid, strerror(errno));
		return ;
	}
	return ;
}



int main(int argc, char **argv)
{
	void	*addr;
	pid_t	pid;
	pid_t	pid1;
	int		new_value;

	if (argc != 5)
	{
		printf("%s\n", "Usage: ./writer PID_MIN PID_MAX addr(HEX) value");
		return (1);
	}
	
	pid = atoi(argv[1]);
	pid1 = atoi(argv[2]);
	addr = (void *)strtoul(argv[3], NULL, 16);
	new_value = atoi(argv[4]);
	while (pid <= pid1)
	{
		change_value(addr, pid, new_value);	
		pid++;
	}
	return (0);
}
