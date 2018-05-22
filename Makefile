######################################################################
# Makefile for EightBall
# Bobbi, 2018
# GPL v3+
######################################################################

# Adjust these to match your site installation
CC65DIR = ~/Personal/Development/cc65
CC65BINDIR = $(CC65DIR)/bin
CC65LIBDIR = $(CC65DIR)/lib
APPLECMDR = ~/Desktop/Apple2/AppleCommander-1.3.5.jar

all: eightball eightballvm disass 8ball20.prg 8ballvm20.prg disass20.prg 8ball64.prg 8ballvm64.prg disass64.prg eightball.system ebvm.system disass.system test.d64 test.dsk

clean:
	rm -f eightball eightballvm disass *.o 8ball20.* 8ball64.* eightball*.s eightball.system test.d64 *.map *.vice

#
# Linux target
#

eightball.o: eightball.c eightballutils.h eightballvm.h
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -c -o eightball.o eightball.c -lm

eightballvm.o: eightballvm.c eightballutils.h eightballvm.h
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -c -o eightballvm.o eightballvm.c -lm

disass.o: disass.c eightballutils.h eightballvm.h
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -c -o disass.o disass.c -lm

eightballutils.o: eightballutils.c eightballutils.h
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -c -o eightballutils.o eightballutils.c -lm

eightball: eightball.o eightballutils.o
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -o eightball eightball.o eightballutils.o -lm

eightballvm: eightballvm.o eightballutils.o
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -o eightballvm eightballvm.o eightballutils.o -lm

disass: disass.o eightballutils.o
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -o disass disass.o eightballutils.o -lm

#
# VIC20 target
#

eightball_20.o: eightball.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t vic20 -D VIC20 -o eightball_20.s eightball.c
	$(CC65BINDIR)/ca65 -t vic20 eightball_20.s

eightballvm_20.o: eightballvm.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t vic20 -D VIC20 -o eightballvm_20.s eightballvm.c
	$(CC65BINDIR)/ca65 -t vic20 eightballvm_20.s

disass_20.o: disass.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t vic20 -D VIC20 -o disass_20.s disass.c
	$(CC65BINDIR)/ca65 -t vic20 disass_20.s

eightballutils_20.o: eightballutils.c eightballutils.h
	$(CC65BINDIR)/cc65 -Or -t vic20 -D VIC20 -o eightballutils_20.s eightballutils.c
	$(CC65BINDIR)/ca65 -t vic20 eightballutils_20.s

8ball20.prg: eightball_20.o eightballutils_20.o
	$(CC65BINDIR)/ld65 -m 8ball20.map -o 8ball20.prg -Ln 8ball20.vice -C vic20-32k.cfg eightball_20.o eightballutils_20.o $(CC65LIBDIR)/vic20.lib

8ballvm20.prg: eightballvm_20.o eightballutils_20.o
	$(CC65BINDIR)/ld65 -m 8ballvm20.map -o 8ballvm20.prg -Ln 8ballvm20.vice -C vic20-32k.cfg eightballvm_20.o eightballutils_20.o $(CC65LIBDIR)/vic20.lib

disass20.prg: disass_20.o eightballutils_20.o
	$(CC65BINDIR)/ld65 -m disass20.map -o disass20.prg -Ln disass20.vice -C vic20-32k.cfg disass_20.o eightballutils_20.o $(CC65LIBDIR)/vic20.lib

#
# C64 target
#

eightball_64.o: eightball.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t c64 -D C64 -o eightball_64.s eightball.c
	$(CC65BINDIR)/ca65 -t c64 eightball_64.s

eightballvm_64.o: eightballvm.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t c64 -D C64 -o eightballvm_64.s eightballvm.c
	$(CC65BINDIR)/ca65 -t c64 eightballvm_64.s

disass_64.o: disass.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t c64 -D C64 -o disass_64.s disass.c
	$(CC65BINDIR)/ca65 -t c64 disass_64.s

eightballutils_64.o: eightballutils.c eightballutils.h
	$(CC65BINDIR)/cc65 -Or -t c64 -D C64 -o eightballutils_64.s eightballutils.c
	$(CC65BINDIR)/ca65 -t c64 eightballutils_64.s

8ball64.prg: eightball_64.o eightballutils_64.o
	$(CC65BINDIR)/ld65 -m 8ball64.map -o 8ball64.prg -Ln 8ball64.vice -C c64.cfg eightball_64.o eightballutils_64.o $(CC65LIBDIR)/c64.lib

8ballvm64.prg: eightballvm_64.o eightballutils_64.o
	$(CC65BINDIR)/ld65 -m 8ballvm64.map -o 8ballvm64.prg -Ln 8ballvm64.vice -C c64.cfg eightballvm_64.o eightballutils_64.o $(CC54LIBDIR)/c64.lib

disass64.prg: disass_64.o eightballutils_64.o
	$(CC65BINDIR)/ld65 -m disass64.map -o disass64.prg -Ln disass64.vice -C c64.cfg disass_64.o eightballutils_64.o $(CC65LIBDIR)/c64.lib

#
# Apple II target
#

eightball_a2e.o: eightball.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t apple2enh -D A2E -o eightball_a2e.s eightball.c
	$(CC65BINDIR)/ca65 -t apple2enh eightball_a2e.s

eightballvm_a2e.o: eightballvm.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t apple2enh -D A2E -o eightballvm_a2e.s eightballvm.c
	$(CC65BINDIR)/ca65 -t apple2enh eightballvm_a2e.s

disass_a2e.o: disass.c eightballutils.h eightballvm.h
	$(CC65BINDIR)/cc65 -Or -t apple2enh -D A2E -o disass_a2e.s disass.c
	$(CC65BINDIR)/ca65 -t apple2enh disass_a2e.s

eightballutils_a2e.o: eightballutils.c eightballutils.h
	$(CC65BINDIR)/cc65 -Or -t apple2enh -D A2E -o eightballutils_a2e.s eightballutils.c
	$(CC65BINDIR)/ca65 -t apple2enh eightballutils_a2e.s

eightball.system: eightball_a2e.o eightballutils_a2e.o
	$(CC65BINDIR)/ld65 -m 8balla2e.map -o eightball.system -C apple2enh-system.cfg eightball_a2e.o eightballutils_a2e.o apple2enh-iobuf-0800.o $(CC65LIBDIR)/apple2enh.lib

ebvm.system: eightballvm_a2e.o eightballutils_a2e.o
	$(CC65BINDIR)/ld65 -m 8ballvma2e.map -o ebvm.system -C apple2enh-system.cfg eightballvm_a2e.o eightballutils_a2e.o apple2enh-iobuf-0800.o $(CC65LIBDIR)/apple2enh.lib

#
disass.system: disass_a2e.o eightballutils_a2e.o
	$(CC65BINDIR)/ld65 -m disassa2e.map -o disass.system -C apple2enh-system.cfg disass_a2e.o eightballutils_a2e.o apple2enh-iobuf-0800.o $(CC65LIBDIR)/apple2enh.lib


#
# EightBall scripts
#

unittest.8bp: unittest.8b
	tr {} [] <unittest.8b | tr \\100-\\132 \\300-\\332 | tr \\140-\\172 \\100-\\132 > unittest.8bp # ASCII -> PETSCII

sieve4.8bp: sieve4.8b
	tr {} [] <sieve4.8b | tr \\100-\\132 \\300-\\332 | tr \\140-\\172 \\100-\\132 > sieve4.8bp # ASCII -> PETSCII

#
# Diskette images
#

test.d64: 8ball20.prg 8ballvm20.prg disass20.prg 8ball64.prg 8ballvm64.prg disass64.prg unittest.8bp sieve4.8bp
	c1541 -format eb,00 d64 test.d64
	c1541 -attach test.d64 -write 8ball20.prg
	c1541 -attach test.d64 -write 8ballvm20.prg
	c1541 -attach test.d64 -write disass20.prg
	c1541 -attach test.d64 -write 8ball64.prg
	c1541 -attach test.d64 -write 8ballvm64.prg
	c1541 -attach test.d64 -write disass64.prg
	c1541 -attach test.d64 -write unittest.8bp unit.8b,s
	c1541 -attach test.d64 -write sieve4.8bp sieve4.8b,s

test.dsk: eightball.system ebvm.system disass.system sieve4.8b tetris.8b bytecode
	java -jar $(APPLECMDR) -d test.dsk e8ball.system
	java -jar $(APPLECMDR) -d test.dsk ebvm.system
	java -jar $(APPLECMDR) -d test.dsk disass.system
	java -jar $(APPLECMDR) -d test.dsk sieve4.8b
	java -jar $(APPLECMDR) -d test.dsk tetris.8b
	java -jar $(APPLECMDR) -d test.dsk bytecode
	java -jar $(APPLECMDR) -p test.dsk e8ball.system sys <eightball.system 
	java -jar $(APPLECMDR) -p test.dsk ebvm.system sys <ebvm.system 
	java -jar $(APPLECMDR) -p test.dsk disass.system sys <disass.system 
	java -jar $(APPLECMDR) -p test.dsk sieve4.8b txt <sieve4.8b
	java -jar $(APPLECMDR) -p test.dsk tetris.8b txt <tetris.8b
	java -jar $(APPLECMDR) -p test.dsk bytecode txt <bytecode

#
# Run emulator with test diskette images
#

xvic: test.d64
	xvic -mem all -drive8type 1541 -8 test.d64

x64: test.d64
	x64 -8 test.d64

mame: test.dsk
	mame -w apple2ee -sl6 diskii -floppydisk1 test.dsk


