#!/bin/sh

nasm -f bin boot1c.asm
nasm -f elf32 -o boot2c.o boot2c.asm

gcc -c -static fdc.c

gcc -c -static fat12.c

ld -T link-pe-i386.ld -o boot.pe boot2c.o fat12.o fdc.o
objcopy -O binary boot.pe boot.obj

dd if=/dev/zero of=fdd.img count=2880
cat boot1c boot.obj | dd of=fdd.img conv=notrunc
