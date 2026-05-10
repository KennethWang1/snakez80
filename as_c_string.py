#!/usr/bin/env python3
import sys

rom = open("./main.rom" if len(sys.argv) == 1 else sys.argv[1], "rb").read()
print(f"PROGMEM const char FULL_CODE[{len(rom)}] = \"{"".join(f"\\x{b:02x}" if b else "\\0" for b in rom)}\";")
