
all:
	zcc +z80 -vn -SO3 -startup=-1 -clib=sdcc_iy --max-allocs-per-node200000 -lm -create-app main.c -o main
	./as_c_string.py main.rom > loader/rom.hpp
loader: ./loader.asm
	z80asm loader.asm -b -o=loader.bin

clean:
	rm -f main main.rom loader.o loader.bin
