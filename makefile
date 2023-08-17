cc = gcc
cflags = -Wall -w

source = ./code/chunk.c ./code/memory.c ./code/value.c ./disassembler/debug.c ./code/compiler.c ./code/vm.c ./code/scanner.c ./code/object.c ./code/table.c

main: $(source) main.c
	$(cc) $(cflags) $(source) main.c -o main

.PHONY: compile
compile: main

.PHONY: run
run: main
	./main $(ARGS)

.PHONY: clean
clean:
	rm -f main

.PHONY: all
all: clean compile run
