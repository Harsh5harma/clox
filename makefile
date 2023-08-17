cc = gcc
cflags = -Wall -w

src_dir = ./code
disasm_dir = ./disassembler
obj_dir = ./obj

source = $(wildcard $(src_dir)/*.c) $(wildcard $(disasm_dir)/*.c)
objects = $(patsubst %.c, $(obj_dir)/%.o, $(notdir $(source)))

main: $(objects) main.c
	$(cc) $(cflags) $(objects) main.c -o main

.PHONY: run
run: main
	./main $(ARGS)

.PHONY: clean
clean:
	rm -f main $(objects)

.PHONY: all
all: clean main run

$(obj_dir)/%.o: $(src_dir)/%.c | $(obj_dir)
	$(cc) $(cflags) -c $< -o $@

$(obj_dir)/%.o: $(disasm_dir)/%.c | $(obj_dir)
	$(cc) $(cflags) -c $< -o $@

$(obj_dir):
	mkdir -p $(obj_dir)
