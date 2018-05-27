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

all: bin/eightball bin/eightballvm bin/disass bin/8ball20.prg bin/8ballvm20.prg bin/disass20.prg bin/8ball64.prg bin/8ballvm64.prg bin/disass64.prg bin/eightball.system bin/ebvm.system bin/disass.system eightball.d64 eightball.dsk

clean:
	rm -f *.s *.o *.map *.vice bin/eightball bin/eightballvm bin/disass bin/*.prg bin/*.system 8b-scripts/*.8bp bytecode eightball.d64

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

bin/eightball: eightball.o eightballutils.o
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -o bin/eightball eightball.o eightballutils.o -lm

bin/eightballvm: eightballvm.o eightballutils.o
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -o bin/eightballvm eightballvm.o eightballutils.o -lm

bin/disass: disass.o eightballutils.o
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -o bin/disass disass.o eightballutils.o -lm

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

bin/8ball20.prg: eightball_20.o eightballutils_20.o
	$(CC65BINDIR)/ld65 -m 8ball20.map -o bin/8ball20.prg -Ln 8ball20.vice -C vic20-32k.cfg eightball_20.o eightballutils_20.o $(CC65LIBDIR)/vic20.lib

bin/8ballvm20.prg: eightballvm_20.o eightballutils_20.o
	$(CC65BINDIR)/ld65 -m 8ballvm20.map -o bin/8ballvm20.prg -Ln 8ballvm20.vice -C vic20-32k.cfg eightballvm_20.o eightballutils_20.o $(CC65LIBDIR)/vic20.lib

bin/disass20.prg: disass_20.o eightballutils_20.o
	$(CC65BINDIR)/ld65 -m disass20.map -o bin/disass20.prg -Ln disass20.vice -C vic20-32k.cfg disass_20.o eightballutils_20.o $(CC65LIBDIR)/vic20.lib

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

bin/8ball64.prg: eightball_64.o eightballutils_64.o
	$(CC65BINDIR)/ld65 -m 8ball64.map -o bin/8ball64.prg -Ln 8ball64.vice -C c64.cfg eightball_64.o eightballutils_64.o $(CC65LIBDIR)/c64.lib

bin/8ballvm64.prg: eightballvm_64.o eightballutils_64.o
	$(CC65BINDIR)/ld65 -m 8ballvm64.map -o bin/8ballvm64.prg -Ln 8ballvm64.vice -C c64.cfg eightballvm_64.o eightballutils_64.o $(CC54LIBDIR)/c64.lib

bin/disass64.prg: disass_64.o eightballutils_64.o
	$(CC65BINDIR)/ld65 -m disass64.map -o bin/disass64.prg -Ln disass64.vice -C c64.cfg disass_64.o eightballutils_64.o $(CC65LIBDIR)/c64.lib

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

bin/eightball.system: eightball_a2e.o eightballutils_a2e.o
	$(CC65BINDIR)/ld65 -m 8balla2e.map -o bin/eightball.system -C apple2enh-system.cfg eightball_a2e.o eightballutils_a2e.o apple2enh-iobuf-0800.o $(CC65LIBDIR)/apple2enh.lib

bin/ebvm.system: eightballvm_a2e.o eightballutils_a2e.o
	$(CC65BINDIR)/ld65 -m 8ballvma2e.map -o bin/ebvm.system -C apple2enh-system.cfg eightballvm_a2e.o eightballutils_a2e.o apple2enh-iobuf-0800.o $(CC65LIBDIR)/apple2enh.lib

bin/disass.system: disass_a2e.o eightballutils_a2e.o
	$(CC65BINDIR)/ld65 -m disassa2e.map -o bin/disass.system -C apple2enh-system.cfg disass_a2e.o eightballutils_a2e.o apple2enh-iobuf-0800.o $(CC65LIBDIR)/apple2enh.lib


#
# EightBall scripts
#

8b-scripts/unittest.8bp: 8b-scripts/unittest.8b
	tr {} [] <8b-scripts/unittest.8b | tr \\100-\\132 \\300-\\332 | tr \\140-\\172 \\100-\\132 > 8b-scripts/unittest.8bp # ASCII -> PETSCII

8b-scripts/sieve.8bp: 8b-scripts/sieve.8b
	tr {} [] <8b-scripts/sieve.8b | tr \\100-\\132 \\300-\\332 | tr \\140-\\172 \\100-\\132 > 8b-scripts/sieve.8bp # ASCII -> PETSCII

8b-scripts/tetris.8bp: 8b-scripts/tetris.8b
	tr {} [] <8b-scripts/tetris.8b | tr \\100-\\132 \\300-\\332 | tr \\140-\\172 \\100-\\132 > 8b-scripts/tetris.8bp # ASCII -> PETSCII

#
# Diskette images
#

eightball.d64: bin/8ball20.prg bin/8ballvm20.prg bin/disass20.prg bin/8ball64.prg bin/8ballvm64.prg bin/disass64.prg 8b-scripts/unittest.8bp 8b-scripts/sieve.8bp 8b-scripts/tetris.8bp
	c1541 -format eb,00 d64 eightball.d64
	c1541 -attach eightball.d64 -write bin/8ball20.prg
	c1541 -attach eightball.d64 -write bin/8ballvm20.prg
	c1541 -attach eightball.d64 -write bin/disass20.prg
	c1541 -attach eightball.d64 -write bin/8ball64.prg
	c1541 -attach eightball.d64 -write bin/8ballvm64.prg
	c1541 -attach eightball.d64 -write bin/disass64.prg
	c1541 -attach eightball.d64 -write 8b-scripts/unittest.8bp unittest.8b,s
	c1541 -attach eightball.d64 -write 8b-scripts/sieve.8bp sieve.8b,s
	c1541 -attach eightball.d64 -write 8b-scripts/tetris.8bp tetris.8b,s

eightball.dsk: bin/eightball.system bin/ebvm.system bin/disass.system 8b-scripts/sieve.8b 8b-scripts/unittest.8b 8b-scripts/tetris.8b
	java -jar $(APPLECMDR) -d eightball.dsk e8ball.system
	java -jar $(APPLECMDR) -d eightball.dsk ebvm.system
	java -jar $(APPLECMDR) -d eightball.dsk disass.system
	java -jar $(APPLECMDR) -d eightball.dsk sieve.8b
	java -jar $(APPLECMDR) -d eightball.dsk unittest.8b
	java -jar $(APPLECMDR) -d eightball.dsk tetris.8b
	java -jar $(APPLECMDR) -d eightball.dsk a2e.auxmem.emd
	java -jar $(APPLECMDR) -p eightball.dsk e8ball.system sys <bin/eightball.system 
	java -jar $(APPLECMDR) -p eightball.dsk ebvm.system sys <bin/ebvm.system 
	java -jar $(APPLECMDR) -p eightball.dsk disass.system sys <bin/disass.system 
	java -jar $(APPLECMDR) -p eightball.dsk sieve.8b txt <8b-scripts/sieve.8b
	java -jar $(APPLECMDR) -p eightball.dsk unittest.8b txt <8b-scripts/unittest.8b
	java -jar $(APPLECMDR) -p eightball.dsk tetris.8b txt <8b-scripts/tetris.8b
	java -jar $(APPLECMDR) -p eightball.dsk a2e.auxmem.emd txt <cc65/a2e.auxmem.emd

#
# Run emulator with test diskette images
#

xvic: eightball.d64
	xvic -mem all -drive8type 1541 -8 eightball.d64

x64: eightball.d64
	x64 -8 eightball.d64

mame: eightball.dsk
	mame -w apple2ee -sl6 diskii -floppydisk1 eightball.dsk

