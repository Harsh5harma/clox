cc = gcc
cflags = -Wall

source = ./code/chunk.c  ./code/memory.c ./code/value.c ./disassembler/debug.c  ./code/compiler.c ./code/vm.c ./code/scanner.c

compile: $(source) main.c
	$(cc) $(cflags) $(source) main.c -o main  

run: ./main
	./main 