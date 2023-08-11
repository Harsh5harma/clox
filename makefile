cc = gcc
cflags = -Wall

source = ./code/chunk.c  ./code/memory.c ./code/value.c ./code/vm.c ./disassembler/debug.c

run: $(source) main.c
	$(cc) $(cflags) $(source) main.c -o main  