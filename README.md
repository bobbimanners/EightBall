# EightBall
The Eight Bit Algorithmic Language for Apple II, Commodore 64 and VIC20

Includes:
- Interpreter
- Compiler
- Virtual Machine

Free Software licenced under GPL v3.
Please see [the Wiki](https://github.com/bobbimanners/EightBall/wiki) for full documentation!!

## The Eight Bit Algorithmic Language for Apple II, Commodore 64 and VIC 20

### What is EightBall?
EightBall is an interpreter for a novel structured programming language.  It runs on a number of 6502-based vintage systems and may also be compiled as a 32 bit Linux executable.

### Design Philosophy
EightBall tries to form a balance of the following qualities, in 20K or so of 6502 code:
* Statically typed
* Provides facilities which encourage structured programming ...
* ... Yet makes it easy to fiddle with hardware (PEEK and POKE and bit twiddling)
* Keep the language as simple and small as possible ...
* ... While providing powerful language primitives and encapsulation in subroutines, allowing the language to be extended by writing EightBall routines
* When in doubt, do it in a similar way to C!

EightBall resembles an interpreted C.

### Supported Systems
The following 6502-based systems are currently supported:
* Apple II - EightBall runs under ProDOS and uses upper/lowercase text.  It should run on 64K Apple IIe, IIc or IIgs.  It can probably run on Apple II/II+ with an 80 column code, but this has not been tested.
* Commodore 64 - EightBall should run on any C64.
* Commodore VIC-20 - EightBall runs on a VIC-20 with 32K of additional RAM.

### Getting Started
There are executables and disk images available to download for Apple II, Commodore 64 and VIC-20.  These may be run on real hardware or one of the many emulators that are available.

The language itself is documented in these wiki pages.  The best way to learn is to study example programs.

### Build Toolchain
I am building EightBall using cc65 v2.15 on Ubuntu Linux.  Please let me know if you need help with compilation.

The Linux version of EightBall is currently being built using gcc 6.3.0.
