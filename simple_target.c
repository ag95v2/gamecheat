#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

int main()
{
	int	*x;
	int	*y;
	int z;
	char	*l;
	size_t n;

	z = 100;
	printf("PID: %d\n", getpid());
	x = malloc(sizeof(int));
	*x = 100;
	y = malloc(sizeof(int));
	*y = 100;
	while (1)
	{
		printf("y = %d\nChange the y value(or press X to skip):\n", *y);
		getline(&l, &n, stdin);
		if (*l == 'X')
			continue ;
		*y = atoi(l);
	}
	return (0);
}
