#include <stdio.h>
#include <stdlib.h>
#include "memmap.h"

#define _GNU_SOURCE
#include <sys/uio.h>

#include <errno.h>
#include <string.h>

#include <unistd.h>


typedef struct s_list
{
	void		*location;
	struct s_list	*next;
}				t_list;

t_list	*lstnew(void *location)
{
	t_list	*l;

	l = malloc(sizeof(t_list));
	if (!l)
		return (NULL);
	l->location = location;
	return (l);
}

t_list *lstadd(t_list *l, void *location)
{
	t_list	*new;

	new = lstnew(location);
	if (!new)
		return (NULL);
	new->next = l;
	return (new);
}

/*
** Return 0 if value found
*/

int scan_location(void *addr, pid_t pid, int value)
{
	struct iovec local[1];
	struct iovec remote[1];
	int          buf1;
	ssize_t      nread;

	int				needed_len = sizeof(int);

	local[0].iov_base = (void *)&buf1;
	local[0].iov_len = needed_len;

	remote[0].iov_base = addr;
	remote[0].iov_len = needed_len;

	nread = process_vm_readv(pid, local, 1, remote, 1, 0);
	if (nread == -1)
	{
		fprintf(stderr, "Failed to read. %s\n", strerror(errno));
		return (1);
	}
	return (value - buf1);
}

t_list	*subset_list(t_list *l, pid_t pid, int new_value)
{
	t_list *prev;
	t_list *current;
	int		count;

	prev = l;
	current = l;
	while (current)
	{
		if (scan_location(current->location, pid, new_value))
		{
			if (prev == current) //We are at start
			{
				current = current->next;
				free(prev);
				prev = current;
				l = current;
				continue ;
			}
			else					//We are not at start
			{
				prev->next = current->next;
				free(current);
				current = prev->next;
				continue ;
			}
		}
		else					// At l_location value changed
		{
			prev = current;
			current = current->next;
		}
	}

	count = 0;
	current = l;
	while (current)
	{
		current = current->next;
		count++;
	}
	printf("Left %d locations! \n", count);
	return (l);
}

/*
** Actually, not only heap, but all except dll and drivers
*/

t_list	*scan_heap(t_memmap *memmap, pid_t pid, int value)
{
	struct iovec local[1];
	struct iovec remote[1];
	char         *buf1;
	ssize_t      nread;

	int				needed_len;
	unsigned long		 pos;
	int				count;
	t_list			*locations;
	t_list			*tmp;


	count = 0;
	locations = NULL;
	tmp = NULL;

	while (memmap )
	{
		if ( memmap->info->type == other || memmap->info->type == shlibs || memmap->info->type == driver_maps )
		{
			memmap = memmap->next;
			continue ;
		}

		needed_len = memmap->info->end - memmap->info->start;
		buf1 = malloc(needed_len);
		if (!buf1)
		{
			fprintf(stderr, "%s\n", "Could not allocate enough memory");
			return (NULL);
		}

		printf("Trying to read PID: %d at %p\n", pid, memmap->info->start);

		local[0].iov_base = buf1;
		local[0].iov_len = needed_len;

		remote[0].iov_base = memmap->info->start;
		remote[0].iov_len = needed_len;

		nread = process_vm_readv(pid, local, 1, remote, 1, 0);
		if (nread == -1)
		{
			fprintf(stderr, "Failed to read. %s\n", strerror(errno));
			memmap = memmap->next;
			continue ;
			//return (NULL);
		}

		printf("Read %ld bytes\n", nread);
		printf("Scanning for value: %d\n", value);

		pos = 0;
		while (pos <= nread - sizeof(int))
		{
			if ((*(int *)(buf1 + pos) == value) || (*(unsigned int*)(buf1 + pos) == (unsigned int)value))
			{
				tmp = lstadd(locations, (void *)(memmap->info->start + pos));
				// Delete the list in normal program
				if (!tmp)
				{
					fprintf(stderr, "Malloc failure. %s\n", strerror(errno));
					return (0);
				}
				locations = tmp;
				count++;
			}
			pos++;
		}
		free(buf1);
		memmap = memmap->next;
	}
	printf("Found %d occurencies. \n", count);
	return (locations);
}

int	lstcount(t_list *l)
{
	int	count;

	count = 0;
	while (l)
	{
		l = l->next;
		count++;
	}
	return (count);
}

enum region_type	where_is(t_memmap *memmap, void *addr)
{
	while (memmap)
	{
		if (addr > memmap->info->start && addr < memmap->info->end)
			return (memmap->info->type);
		memmap = memmap->next;
	}
	return (other);
}

void	value_summary(t_memmap *memmap, t_list *addrs)
{
	int	h, st, bss, anon, oth;
	enum region_type t;	

	oth = h = st = bss= anon = 0;
	while (addrs)
	{
		printf("%p", addrs->location);
		t = where_is(memmap, addrs->location);
		if (t == heap)
		{
			h++;
			printf(" ->heap\n");
		}
		else if (t == stack)
		{
			st++;
			printf(" ->stack\n");
		}
		else if (t == static_storage)
		{
			bss++;
			printf(" ->static storage\n");
		}
		else if (t == anon_maps)
		{
			anon++;
			printf(" ->anon_maps\n");
		}
		else
		{
			oth++;
			printf(" ->other\n");
		}
		addrs = addrs->next;
	}
	printf("SUMMARY\nHeap: %d\nStack: %d\nStatic storage: %d\nAnon maps: %d\nOther: %d\n", h,st,bss,anon, oth);
}

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
		fprintf(stderr, "Failed to read. %s\n", strerror(errno));
		return ;
	}
	return ;
}


void change_everywhere(t_list *addrs, int new_value, t_memmap *memmap, pid_t pid)
{
	enum region_type t;

	while (addrs)
	{
		t = where_is(memmap, addrs->location);	
		if (t == heap || t == stack || t == static_storage || t == anon_maps)
			change_value(addrs->location, pid, new_value);
		addrs = addrs->next;
	}
}


void	repl()
{
	char	*l;
	size_t	len;
	pid_t	pid;
	int		val;
	t_memmap *memmap;
	t_list	*addrs;
	int	count;

	l = 0;
	printf("%s\n", "Enter the PID:");
	getline(&l, &len, stdin);
	pid = atoi(l);
	memmap = get_memory_map(pid);
	print_map_info(memmap);

	printf("%s\n", "Enter the deicmal value to search for:");
	getline(&l, &len, stdin);
	val = atoi(l);

	addrs = scan_heap(memmap, pid, val);

	while ((count = lstcount(addrs)) )
	{
		printf("%s\n", "Enter one more value to be sure (or s value if you are ready to risk!):");
		getline(&l, &len, stdin);
		if (*l == 's')
		{
			// Command to change everywhere. Very risky!!!
			val = atoi(l + 1);
			change_everywhere(addrs, val, memmap, pid);
			continue ;
		}

		val = atoi(l);
				
		printf("Searching now for value: %d\n", val);
		addrs = subset_list(addrs, pid, val);
		value_summary(memmap, addrs);
	}
	free(l);
}

int	main()
{
	repl();
	return (0);
}
