#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

int main()
{
	double  *x;
	char	*l;
	size_t n;

	printf("PID: %d\n", getpid());
	x = malloc(sizeof(double));
	*x = 100.0;
	l = 0;
	while (1)
	{
		printf("x = %f\nChange the x value(or press X to skip):\n", *x);
		getline(&l, &n, stdin);
		if (*l == 'X')
			continue ;
		*x = strtod(l, NULL);
	}
	free(l);
	return (0);
}
