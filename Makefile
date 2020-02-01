all:
	gcc -g -Wall -Wextra repl.c memmap.c -I . -o prototype
	gcc -g -Wall -Wextra repl_dbl.c memmap.c -I . -o prototype_dbl
