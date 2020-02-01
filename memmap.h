#ifndef MEMMAP_H
#define MEMMAP_H

#define R 1
#define W 2
#define X 4
#define P 8

#include <sys/types.h>

enum region_type
{
	static_storage,
	heap,
	stack,
	anon_maps,
	shlibs,
	driver_maps,
	other
};

typedef struct s_region
{
	void *           start;
	void *           end;
	char             perms;
	unsigned long    offset;
	int              dev_major;
	int              dev_minor;
	int              inode;
	char *           pathname;
	enum region_type type;
} t_region;

typedef struct s_memmap
{
	t_region *       info;
	struct s_memmap *next;
} t_memmap;

t_memmap *get_memory_map(pid_t pid);
void print_map_info(t_memmap *map);
#endif
