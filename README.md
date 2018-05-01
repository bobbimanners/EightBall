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

### Simple Subroutine Declaration

EightBall allows named subroutines to be defined, for example:

    sub myFirstSubroutine()
      pr.msg "Hello"; pr.nl
    endsub

All subroutines must end with `endsub` statement.

A subroutine may have more than one `return` statement returning a numeric value:

    sub mySecondSubroutine()
      return 2
    endsub

If the flow of execution hits the `endsub` then 0 is returned to the caller.

### Simple Subroutine Invocation

The subroutine above can be called as follows:

    call myFirstSubroutine()

When `myFirstSubroutine` hits a `return` statement, the flow of execution will return to the statement immediately following the `call`.

### Local Variables

Each subroutine has its own local variable scope.  If a local variable is declared with the same name as a global variable, the global will not be available within the scope of the subroutine.  When the subroutine returns, the local variables are destroyed.

    sub myThirdSubroutine()
      word w[10] = 0;  ' Local array
    endsub

### Argument Passing

Subroutines may take `byte` or `word` arguments, using the following syntax:

    sub withArgs(byte flag, word val1, word val2)
      ' Do stuff
      return 0
    endsub

This could be called as follows:

    word ww = 0; byte b = 0;
    call withArgs(b, ww, ww+10)

When `withArgs` runs, the expression passed as the first argument (`b`) will be evaluated and the value assigned to the first formal argument `flag`, which will be created in the subroutine's local scope.  Similarly, the second argument (`ww`) will be evaluated and the result assigned to `val1`. Finally, `ww+10` will be evaluated and assigned to `val2`.

Argument passing is by value, which means that `withArgs` can modify `flag`, `val1` or `val2` freely without the changes being visible to the caller.

### Function Invocation

Subroutines may be invoked within an expression.  For example, the following subroutine:

    sub adder(word a, word b)
      return a+b
    endsub

Could be used in an expression like this:

    pr.dec adder(10, 5); ' Prints 15

Functions may invoke themselves recursively (but you will run out of stack quite fast!)

### Passing by Reference

Passing by reference allows a subroutine to modify a value passed to it.  EightBall does this using pointers, in a manner that will be familiar to C programmers.  Here is `adder` implemented using this pattern:

    sub adder(word a, word b, word resptr)
      *resptr = a+b
    endsub

Then to call it:

    word result
    call adder(10, 20, &result)

This code takes the address of variable `result` using the ampersand operator and passes it to subroutine `adder` as `resptr`.  The subroutine then uses the star operator to write the result of the addition of the first two arguments (10 + 20 in this example) to the word pointed to by `resptr`.

Unlike C, there are no special pointer types.  Pointers must be stored in a `word` variable, since they do not fit in a `byte`.

### Passing an Array by Reference

**Warning: This is currently not implemented in the compiler, only the interpreter.**

It is frequently useful to pass an array into a subroutine.  It is not very useful to use pass by value for arrays, since this may mean copying a large object onto the stack.  For these reasons, EightBall implements a special pass by reference mode for array variables, which operates in a manner similar to C.

Here is an example of a function which takes a regular variable and an array:

    sub clearArray(byte arr[], word sz)
      word i = 0
      for i = 0 : sz-1
        arr[i] = 0
      endfor
    endsub

This may be invoked like this:

    word n = 10
    byte A[n] = 99
    call clearArray(A, n)

Note that the size of the array is not specified in the subroutine definition - any size array may be passed.  Note also that the corresponding argument in the `call` is simply the array name (no [] or other annotation is permitted.)

This mechanism effectively passes a pointer to the array contents 'behind the scenes'.

### End Statement
The `end` statement marks the normal end of execution.  This is often used to stop the flow of execution running off the end of the main program and into the subroutines (which causes an error):

    call foo()
    pr.msg "Done!"; pr.nl
    end
    sub foo()
      pr.msg "foo"; pr.nl
    endsub


