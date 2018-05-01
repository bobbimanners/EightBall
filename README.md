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

### Simple Types

EightBall has two basic types: byte (8 bits) and word (16 bits).

    word counter = 1000
    byte xx = 0

Variables must be declared before use.  Variables **must** be initialized.

The first four letters of the variable name are significant, any letters after that are simply ignored by the parser.

Variables of type word are also used to store pointers (there is no pointer type in EightBall).

### Arrays

Arrays of byte and word may be declared as follows.  The mandatory initializer is used to initialize all elements:

    word myArray[100] = 1
    byte storage[10] = 0

**_At present, only 1D arrays are supported, but this will be expanded in future releases._**

**Array dimensions must be literal constants. Expressions are not parsed in this case.**

Array elements begin from 0, so the array `storage` above has elements from 0 to 9.

    storage[0] = 0;  ' First element
    storage[9] = 99; ' Last element
    
EightBall supports a 'structured' programming style by providing multi-line `if`/`then`/`else` conditionals, `for` loops and `while` loops.

Note that the `goto` statement is not supported!

### Conditionals
Syntax is as follows:

    if z == 16
      pr.msg "Sweet sixteen!"
      pr.nl
    endif
Or, with the optional `else` clause:

    if x < 2
      pr.msg "okay"
      pr.nl
    else
      pr.msg "too many"; pr.nl
      ++toomany;
    endif

### For Loops
Syntax is as per the following example:

    for count = 1 : 10
      pr.dec count
      pr.nl
    endfor

### While Loops
These are quite flexible, for example:

    while bytes < 255
      call getbyte()
      bytes = bytes + 1
    endwhile

