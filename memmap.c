#include "memmap.h"
#include <stdlib.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <ctype.h>

#define MAX_NAME_LEN 20
#define HEX 16

/*
** Line ends with '\n\0'
** Format is hard-coded inside linux kernel!
** start-end ...bla bla bla
*/

t_region *parse_region(char *line)
{
	char *    start;
	char *    end;
	t_region *region;

	region = malloc(sizeof(t_region));
	if (!region)
	{
		perror("Could not allocate memory");
		return (NULL);
	}
	start = line;
	region->start = (void *)strtoul(start, &end, HEX);
	start = end + 1;
	region->end = (void *)strtoul(start, &end, HEX);

	start = end + 1;
	while (isspace(*start))
		start++;

	/* start-end rwxp */
	region->perms = 0;
	if (*start == 'r')
		region->perms |= R;
	start++;
	if (*start == 'w')
		region->perms |= W;
	start++;
	if (*start == 'x')
		region->perms |= R;
	start++;
	if (*start == 'p')
		region->perms |= P;
	start++;

	while (isspace(*start))
		start++;

	/* OFFSET */
	region->offset = strtoul(start, &end, HEX);

	/* Device numbers */
	region->dev_major = atoi(end);
	while (*end != ':')
		end++;
	end++;
	region->dev_minor = atoi(end);
	while (!isspace(*end))
		end++;

	/* Inode number */
	region->inode = strtoul(end, &start, HEX);
	while (isspace(*start))
		start++;

	/* Pathname */
	end = start;
	while (*end != '\n' && *end)
		end++;
	*end = 0;
	region->pathname = strdup(start);

	/* Type */
	if (!strcmp(region->pathname, "[heap]"))
		region->type = heap;
	else if (strstr(region->pathname, "[stack"))
		region->type = stack;
	else if (strstr(region->pathname, ".so"))
		region->type = shlibs;
	else if (strstr(region->pathname, "/dev/"))
		region->type = driver_maps;
	else if (strstr(region->pathname, "[v"))
		region->type = other;
	else if (!region->inode)
		region->type = anon_maps;
	else
		region->type = static_storage;
	return (region);
}

void print_region_info(t_region *region)
{
	char thousands;
	int  number;

	number = region->end - region->start;
	thousands = 'b';
	if ((region->end - region->start) / 1000)
	{
		thousands = 'K';
		number = (region->end - region->start) / 1000;
	}
	if ((region->end - region->start) / 1000000)
	{
		thousands = 'M';
		number = (region->end - region->start) / 1000000;
	}

	if (region->type == static_storage)
		printf("%16s ", "Static storage");
	if (region->type == heap)
		printf("%16s ", "Heap");
	if (region->type == anon_maps)
		printf("%16s ", "Anonimous map");
	if (region->type == driver_maps)
		printf("%16s ", "Driver map");
	if (region->type == stack)
		printf("%16s ", "Stack");
	if (region->type == shlibs)
		printf("%16s ", "Shared libs");

	printf("%5d%c from %20p to %20p %s\n",
	       number,
	       thousands,
	       region->start,
	       region->end,
	       region->pathname);
}

void del_region(t_region *reg)
{
	if (!reg)
		return;
	free(reg->pathname);
	free(reg);
}

void del_map(t_memmap *map)
{
	t_memmap *tmp;

	while (map)
	{
		tmp = map;
		del_region(map->info);
		map = map->next;
		free(tmp);
	}
}

/*
** Parse a line and add to the beginning of linked list
** Return a pointer to new start or NULL in case of malloc errors
*/

t_memmap *add_region(t_memmap *map, char *line)
{
	t_region *reg;
	t_memmap *new;

	if (!(reg = parse_region(line)))
		return (NULL);
	if (!(new = malloc(sizeof(t_memmap))))
		return (NULL);
	new->info = reg;
	new->next = map;
	return (new);
}

/*
** Create a memory map of process by PID
*/

t_memmap *get_memory_map(pid_t pid)
{
	char      fname[MAX_NAME_LEN];
	size_t    n;
	FILE *    stream;
	char *    line;
	ssize_t   len;
	t_memmap *memmap;
	t_memmap *start;

	n = snprintf(fname, MAX_NAME_LEN - 1, "/proc/%d/maps", pid);
	if (n >= MAX_NAME_LEN - 1 || (size_t)n <= sizeof("/proc//maps"))
	{
		fprintf(stderr, "Error: PID %d looks invalid \n", pid);
		return (NULL);
	}
	stream = fopen(fname, "r");
	if (!stream)
	{
		fprintf(stderr, "Could not open %s: %s", fname, strerror(errno));
		return (NULL);
	}

	/* Fill the memmap */
	memmap = NULL;
	line = NULL;
	while ((len = getline(&line, &n, stream)) != -1)
	{
		if (!(start = add_region(memmap, line)))
		{
			del_map(memmap);
			fprintf(stderr, "%s", "Memory allocation failure\n");
			return (NULL);
		}
		memmap = start;
	}
	free(line);
	fclose(stream);
	return (memmap);
}

void print_map_info(t_memmap *map)
{
	unsigned long s_s, h, st, shl, dev, total, anon;

	anon = s_s = h = st = shl = dev = total = 0;
	while (map)
	{
		if (map->info->type == static_storage)
			s_s += map->info->end - map->info->start;
		if (map->info->type == heap)
			h += map->info->end - map->info->start;
		if (map->info->type == shlibs)
			shl += map->info->end - map->info->start;
		if (map->info->type == driver_maps)
			dev += map->info->end - map->info->start;
		if (map->info->type == stack)
			st += map->info->end - map->info->start;
		if (map->info->type == anon_maps)
			anon += map->info->end - map->info->start;

		map = map->next;
	}
	total = s_s + st + h + dev + shl + anon;
	printf("%15s: %8.2f Mb\n%15s: %8.2f Mb\n%15s: %8.2f Mb\n%15s: "
	       "%8.2f Mb\n%15s: %8.2f Mb\n%15s: %8.2f Mb\n%15s: %8.2f Mb\n",
	       "Static storage",
	       ((float)s_s) / 1000000,
	       "Heap",
	       ((float)h) / 1000000,
	       "Stack",
	       ((float)st) / 1000000,
	       "Shared libs",
	       ((float)shl) / 1000000,
	       "Drivers",
	       ((float)dev) / 1000000,
		   "Anonimous maps",
		   ((float)anon) / 1000000,
	       "Total",
	       ((float)total) / 1000000);
}
