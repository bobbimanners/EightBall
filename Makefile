all: eightball eightballvm 8ball20.prg 8ballvm20.prg 8ball64.prg 8ballvm64.prg eightball.system ebvm.system test.d64 test.dsk

clean:
	rm -f eightball eightballvm *.o 8ball20.* 8ball64.* eightball*.s eightball.system test.d64 *.map

eightball.o: eightball.c eightballutils.h eightballvm.h
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -c -o eightball.o eightball.c -lm

eightballvm.o: eightballvm.c eightballutils.h eightballvm.h
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -c -o eightballvm.o eightballvm.c -lm

eightballutils.o: eightballutils.c eightballutils.h
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -c -o eightballutils.o eightballutils.c -lm

eightball: eightball.o eightballutils.o
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -o eightball eightball.o eightballutils.o -lm

eightballvm: eightballvm.o eightballutils.o
	# 32 bit so sizeof(int*) = sizeof(int) [I am lazy]
	gcc -m32 -Wall -Wextra -g -o eightballvm eightballvm.o eightballutils.o -lm

eightball_20.o: eightball.c eightballutils.h eightballvm.h
	~/Personal/Development/cc65/bin/cc65 -Or -t vic20 -D VIC20 -o eightball_20.s eightball.c
	~/Personal/Development/cc65/bin/ca65 -t vic20 eightball_20.s

eightballvm_20.o: eightballvm.c eightballutils.h eightballvm.h
	~/Personal/Development/cc65/bin/cc65 -Or -t vic20 -D VIC20 -o eightballvm_20.s eightballvm.c
	~/Personal/Development/cc65/bin/ca65 -t vic20 eightballvm_20.s

eightballutils_20.o: eightballutils.c eightballutils.h
	~/Personal/Development/cc65/bin/cc65 -Or -t vic20 -D VIC20 -o eightballutils_20.s eightballutils.c
	~/Personal/Development/cc65/bin/ca65 -t vic20 eightballutils_20.s

8ball20.prg: eightball_20.o eightballutils_20.o
	~/Personal/Development/cc65/bin/ld65 -m 8ball20.map -o 8ball20.prg -Ln 8ball20.vice -C vic20-32k.cfg eightball_20.o eightballutils_20.o ~/Personal/Development/cc65/lib/vic20.lib

8ballvm20.prg: eightballvm_20.o eightballutils_20.o
	~/Personal/Development/cc65/bin/ld65 -m 8ballvm20.map -o 8ballvm20.prg -Ln 8ballvm20.vice -C vic20-32k.cfg eightballvm_20.o eightballutils_20.o ~/Personal/Development/cc65/lib/vic20.lib

eightball_64.o: eightball.c eightballutils.h eightballvm.h
	~/Personal/Development/cc65/bin/cc65 -Or -t c64 -D C64 -o eightball_64.s eightball.c
	~/Personal/Development/cc65/bin/ca65 -t c64 eightball_64.s

eightballvm_64.o: eightballvm.c eightballutils.h eightballvm.h
	~/Personal/Development/cc65/bin/cc65 -Or -t c64 -D C64 -o eightballvm_64.s eightballvm.c
	~/Personal/Development/cc65/bin/ca65 -t c64 eightballvm_64.s

eightballutils_64.o: eightballutils.c eightballutils.h
	~/Personal/Development/cc65/bin/cc65 -Or -t c64 -D C64 -o eightballutils_64.s eightballutils.c
	~/Personal/Development/cc65/bin/ca65 -t c64 eightballutils_64.s

8ball64.prg: eightball_64.o eightballutils_64.o
	~/Personal/Development/cc65/bin/ld65 -m 8ball64.map -o 8ball64.prg -Ln 8ball64.vice -C c64.cfg eightball_64.o eightballutils_64.o ~/Personal/Development/cc65/lib/c64.lib

8ballvm64.prg: eightballvm_64.o eightballutils_64.o
	~/Personal/Development/cc65/bin/ld65 -m 8ballvm64.map -o 8ballvm64.prg -Ln 8ballvm64.vice -C c64.cfg eightballvm_64.o eightballutils_64.o ~/Personal/Development/cc65/lib/c64.lib

eightball_a2e.o: eightball.c eightballutils.h eightballvm.h
	~/Personal/Development/cc65/bin/cc65 -Or -t apple2enh -D A2E -o eightball_a2e.s eightball.c
	~/Personal/Development/cc65/bin/ca65 -t apple2enh eightball_a2e.s

eightballvm_a2e.o: eightballvm.c eightballutils.h eightballvm.h
	~/Personal/Development/cc65/bin/cc65 -Or -t apple2enh -D A2E -o eightballvm_a2e.s eightballvm.c
	~/Personal/Development/cc65/bin/ca65 -t apple2enh eightballvm_a2e.s

eightballutils_a2e.o: eightballutils.c eightballutils.h
	~/Personal/Development/cc65/bin/cc65 -Or -t apple2enh -D A2E -o eightballutils_a2e.s eightballutils.c
	~/Personal/Development/cc65/bin/ca65 -t apple2enh eightballutils_a2e.s

eightball.system: eightball_a2e.o eightballutils_a2e.o
	~/Personal/Development/cc65/bin/ld65 -m 8balla2e.map -o eightball.system -C apple2enh-system.cfg eightball_a2e.o eightballutils_a2e.o apple2enh-iobuf-0800.o ~/Personal/Development/cc65/lib/apple2enh.lib

ebvm.system: eightballvm_a2e.o eightballutils_a2e.o
	~/Personal/Development/cc65/bin/ld65 -m 8ballvma2e.map -o ebvm.system -C apple2enh-system.cfg eightballvm_a2e.o eightballutils_a2e.o apple2enh-iobuf-0800.o ~/Personal/Development/cc65/lib/apple2enh.lib

test.d64: 8ball20.prg 8ballvm20.prg 8ball64.prg 8ballvm64.prg
	c1541 -format eb,00 d64 test.d64
	c1541 -attach test.d64 -write 8ball20.prg
	c1541 -attach test.d64 -write 8ballvm20.prg
	c1541 -attach test.d64 -write 8ball64.prg
	c1541 -attach test.d64 -write 8ballvm64.prg

test.dsk: eightball.system ebvm.system sieve4.8b
	java -jar ~/Desktop/Apple2/AppleCommander-1.3.5.jar  -d test.dsk e8ball.system
	java -jar ~/Desktop/Apple2/AppleCommander-1.3.5.jar  -d test.dsk ebvm.system
	java -jar ~/Desktop/Apple2/AppleCommander-1.3.5.jar  -d test.dsk sieve4.8b
	java -jar ~/Desktop/Apple2/AppleCommander-1.3.5.jar  -p test.dsk e8ball.system sys <eightball.system 
	java -jar ~/Desktop/Apple2/AppleCommander-1.3.5.jar  -p test.dsk ebvm.system sys <ebvm.system 
	java -jar ~/Desktop/Apple2/AppleCommander-1.3.5.jar  -p test.dsk sieve4.8b txt <sieve4.8b

