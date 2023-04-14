server:
	gcc -Wall main.c scanner.c compiler.c vm.c chunk.c memory.c debug.c value.c table.c -o clox