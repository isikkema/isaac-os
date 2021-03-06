CC=riscv64-unknown-linux-gnu-gcc
GDB=riscv64-unknown-linux-gnu-gdb

CFLAGS=-g -O2 -Wall -Wextra -march=rv64gc -mabi=lp64d -ffreestanding -nostdlib -nostartfiles -Isrc/include -mcmodel=medany
LDFLAGS=-Tlds/riscv.lds

SOURCES=$(wildcard src/*.c)
SOURCES_ASM=$(wildcard asm/*.S)
OBJECTS=$(patsubst src/%.c,objs/%.o,$(SOURCES))
OBJECTS+= $(patsubst asm/%.S,objs/%.o,$(SOURCES_ASM))
DEPS=$(patsubst src/%.c,deps/%.d,$(SOURCES))

QEMU_OPTIONS= -serial mon:stdio -gdb unix:debug.pipe,server,nowait
QEMU_DEVICES+= -device pcie-root-port,id=rp1,multifunction=off,chassis=0,slot=1,bus=pcie.0,addr=01.0
QEMU_DEVICES+= -device pcie-root-port,id=rp2,multifunction=off,chassis=1,slot=2,bus=pcie.0,addr=02.0
QEMU_DEVICES+= -device pcie-root-port,id=rp3,multifunction=off,chassis=2,slot=3,bus=pcie.0,addr=03.0
QEMU_DEVICES+= -device virtio-tablet,bus=rp1,id=tablet
QEMU_DEVICES+= -device virtio-keyboard,bus=rp1,id=keyboard
QEMU_DEVICES+= -device virtio-gpu-pci,bus=rp2,id=gpu
QEMU_DEVICES+= -device virtio-rng-pci-non-transitional,bus=rp1,id=rng
QEMU_DEVICES+= -device virtio-blk-pci-non-transitional,drive=foo0,bus=rp2,id=blk0
QEMU_DEVICES+= -device virtio-blk-pci-non-transitional,drive=foo1,bus=rp2,id=blk1
QEMU_DEVICES+= -device qemu-xhci,bus=rp3,id=usbhost
QEMU_DEVICES+= -drive if=none,format=raw,file=hdd0.dsk,id=foo0
QEMU_DEVICES+= -drive if=none,format=raw,file=hdd1.dsk,id=foo1

all: cosc562.elf

run: cosc562.elf
	$(MAKE) -C sbi
	qemu-system-riscv64 -nographic -bios ./sbi/sbi.elf -d guest_errors,unimp -cpu rv64 -machine virt -smp 8 -m 256M -kernel cosc562.elf $(QEMU_OPTIONS) $(QEMU_DEVICES)

rung: cosc562.elf
	$(MAKE) -C sbi
	qemu-system-riscv64 -bios ./sbi/sbi.elf -d guest_errors,unimp -cpu rv64 -machine virt -smp 8 -m 256M -kernel cosc562.elf $(QEMU_OPTIONS) $(QEMU_DEVICES)

cosc562.elf: $(OBJECTS) lds/riscv.lds
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJECTS)

objs/%.o: src/%.c Makefile
	$(CC) -MD -MF ./deps/$*.d $(CFLAGS) -o $@ -c $<

objs/%.o: asm/%.S Makefile
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean gdb run

gdb: cosc562.elf
	$(GDB) $< -ex "target extended-remote debug.pipe"

fs: hdd0.dsk hdd1.dsk

hdd0.dsk: minix3.dsk
	dd conv=notrunc if=minix3.dsk of=hdd0.dsk

hdd1.dsk: ext4.dsk
	dd conv=notrunc if=ext4.dsk of=hdd1.dsk

clean:
	rm -f objs/*.o deps/*.d cosc562.elf
	make -C sbi clean
