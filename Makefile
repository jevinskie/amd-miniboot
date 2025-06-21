CROSS  ?= x86_64-unknown-linux-gnu-
CC      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS  = -Oz -ffreestanding -fno-stack-protector -mno-red-zone \
          -mcmodel=large -nostdlib -Wall -Wextra
LDFLAGS = -T linker.ld -z max-page-size=4096

OBJS = entry64.o rtl8139_fifo.o net_tiny.o stage0.o

stage0.bin: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o stage0.elf
	$(OBJCOPY) -O binary stage0.elf $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf stage0.bin
