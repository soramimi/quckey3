# Hey Emacs, this is a -*- makefile -*-
#----------------------------------------------------------------------------
# WinAVR Makefile Template written by Eric B. Weddington, Jörg Wunsch, et al.
#
# Released to the Public Domain
#
# Additional material for this makefile was written by:
# Peter Fleury
# Tim Henigan
# Colin O'Flynn
# Reiner Patommel
# Markus Pfaff
# Sander Pool
# Frederik Rouleau
# Carlos Lamas
#
#
# Fredrik Atmer then deleted most of the content for the AVR-Keyboard
#
#----------------------------------------------------------------------------
# On command line:
#
# make = Make software.
#
# make clean = Clean out built project files.
#
# To rebuild project do "make clean" then "make all".
#----------------------------------------------------------------------------

# Keyboard type (with micro controller code and speed)
#BOARD = phantom
#LAYOUT = ansi_iso
#LAYOUT = ansi_iso_win
#MCU = atmega32u4
#F_CPU = 16000000
#B_LOADER = \"jmp\ 0x7E00\"

# BOARD = hid_liber
# LAYOUT = ansi_iso_jis
# MCU = atmega32u2
# F_CPU = 16000000
# B_LOADER = \"jmp\ 0x7000\"

BOARD = .
LAYOUT = test
MCU = atmega32u2
F_CPU = 16000000
B_LOADER = \"jmp\ 0x7000\"

#BOARD = sskb
#LAYOUT = symmetric
#LAYOUT = iso
#MCU = at90usb1286
#F_CPU = 16000000
#B_LOADER = \"jmp\ 0x1FC00\"

#BOARD = pontus
#LAYOUT = pontus
#MCU = at90usb1286
#F_CPU = 16000000
#B_LOADER = \"jmp\ 0x1FC00\"

# List C source files here.
#SRC =	main.c \
#	usb.c \
#	board.c

#	print.c \

#CDEFS = -DF_CPU=$(F_CPU)UL -D__INCLUDE_KEYBOARD=\"$(BOARD)/board.h\" -D__INCLUDE_LAYOUT=\"$(BOARD)/$(LAYOUT).h\" -D__BOOTLOADER_JUMP=$(B_LOADER)

# Optimization level, can be [0, 1, 2, 3, s]. 
#     0 = turn off optimization. s = optimize for size.
#     (Note: 3 is not always the best optimization level. See avr-libc FAQ.)
#OPTLEVEL = 2

#---------------- Compiler Options C ----------------
#  -g*:          generate debugging information
#  -O*:          optimization level
#  -f...:        tuning, see GCC manual and avr-libc documentation
#  -Wall...:     warning level
#  -Wa,...:      tell GCC to pass this to the assembler.
#    -adhlns...: create assembler listing
#CFLAGS = $(CDEFS)
#CFLAGS += -O$(OPTLEVEL)
#CFLAGS += -ffunction-sections
#CFLAGS += -Wall
#CFLAGS += -Wa,-adhlns=$(<:%.c=%.lst)
#CFLAGS += -std=gnu99

#---------------- Linker Options ----------------
#  -Wl,...:     tell GCC to pass this to linker.
#    -Map:      create map file
#    --cref:    add cross reference to  map file
LDFLAGS = -Os -Wl,-Map=avr_keyboard.map,--cref
LDFLAGS += -Wl,--relax
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -lm

# Define programs and commands.
SHELL = sh

# Define all object files.
OBJ = \
	lcd.o \
	ps2dec.o \
	ps2dec001.o \
	ps2dec002.o \
	ps2if.o \
	quckey.o \
	queue16.o \
	usb.o \
	main.o \
	waitloop.o

CFLAGS = -Os -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Wall -Wextra -Werror=return-type
CC = avr-gcc $(CFLAGS)
CXX = avr-g++ $(CFLAGS) -std=c++11

# Change the build target to build a HEX file or a library.
build: avr_keyboard.hex avr_keyboard.eep end

# Create final output files (.hex, .eep) from ELF output file.
%.hex: %.elf
	@echo
	@echo Creating load file for Flash: $@
	avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@

%.eep: %.elf
	@echo
	@echo Creating load file for EEPROM: $@
	-avr-objcopy -j .eeprom --set-section-flags=.eeprom="alloc,load" \
	--change-section-lma .eeprom=0 --no-change-warnings -O ihex $< $@ || exit 0

# Create library from object files.
%.a: $(OBJ)
	@echo
	@echo Creating library: $@
	avr-ar rcs $@ $(OBJ)

# Link: create ELF output file from object files.
%.elf: $(OBJ)
	@echo
	@echo Linking: $@
	avr-g++ -mmcu=$(MCU) $^ --output $@ $(LDFLAGS)

# Compile: create object files from C source files.
#%.o : %.c
#	@echo
#	@echo Compiling C: $<
#	avr-gcc -c $(ALL_CFLAGS) $< -o $@ 

end: 
	@echo
	find . -regextype posix-awk -regex \
		"(.*\.cof|.*\.elf|.*\.map|.*\.sym|.*\.lss|.*\.o|.*\.lst|.*\.s|.*\.d|.*\.i)" \
		-exec rm {} +
	rm -rf .dep

clean:
	@echo
	@echo Cleaning everything:
	find . -maxdepth 1 -regextype posix-awk -regex \
	        "(.*\.hex|.*\.eep|.*\.cof|.*\.elf|.*\.map|.*\.sym|.*\.lss|.*\.o|.*\.lst|.*\.s|.*\.d|.*\.i)" \
		-exec rm {} +
	rm -rf .dep

# Include the dependency files.
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)

.PHONY : build hex eep end clean

write: avr_keyboard.hex
	ftavr -w avr_keyboard.hex --avr-write-fuse-e f4 --avr-write-fuse-h d9 --avr-write-fuse-l 5e

