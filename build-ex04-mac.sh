#!/bin/sh

nasm -f bin boot1c.asm
nasm -f elf32 -DNO_ -o boot2c.o boot2c.asm

i686-elf-gcc -c -static fdc.c

i686-elf-gcc -c -static fat12.c

i686-elf-ld -T link-binary.ld -o boot.obj boot2c.o fat12.o fdc.o

dd if=/dev/zero of=fdd.img count=2880
cat boot1c boot.obj | dd of=fdd.img conv=notrunc
