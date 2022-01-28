CC=riscv64-unknown-linux-gnu-gcc
GDB=riscv64-unknown-linux-gnu-gdb

CFLAGS=-g -O0 -Wall -Wextra -march=rv64gc -mabi=lp64d -ffreestanding -nostdlib -nostartfiles -Isrc/include -mcmodel=medany
LDFLAGS=-Tlds/sbi.lds

SOURCES=$(wildcard src/*.c)
SOURCES_ASM=$(wildcard asm/*.S)
OBJECTS=$(patsubst src/%.c,objs/%.o,$(SOURCES))
OBJECTS+= $(patsubst asm/%.S,objs/%.o,$(SOURCES_ASM))
DEPS=$(patsubst src/%.c,deps/%.d,$(SOURCES))

all: sbi.elf

sbi.elf: $(OBJECTS) lds/sbi.lds
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJECTS) $(LIBS)

objs/%.o: src/%.c Makefile
	$(CC) -MD -MF ./deps/$*.d $(CFLAGS) -o $@ -c $<

objs/%.o: asm/%.S Makefile
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean gdb

gdb: sbi.elf
	$(GDB) $< -ex "target extended-remote debug.pipe"

clean:
	rm -f objs/*.o deps/*.d sbi.elf
