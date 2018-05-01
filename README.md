# The Eight Bit Algorithmic Language for Apple II, Commodore 64 and VIC 20

The Eight Bit Algorithmic Language for Apple II, Commodore 64 and VIC20

Includes:
- Interpreter
- Compiler
- Virtual Machine

## Licence

Free Software licenced under GPL v3.
Please see [the Wiki](https://github.com/bobbimanners/EightBall/wiki) for full documentation!!

# Intro

## What is EightBall?
EightBall is an interpreter for a novel structured programming language.  It runs on a number of 6502-based vintage systems and may also be compiled as a 32 bit Linux executable.

## Design Philosophy
EightBall tries to form a balance of the following qualities, in 20K or so of 6502 code:
* Statically typed
* Provides facilities which encourage structured programming ...
* ... Yet makes it easy to fiddle with hardware (PEEK and POKE and bit twiddling)
* Keep the language as simple and small as possible ...
* ... While providing powerful language primitives and encapsulation in subroutines, allowing the language to be extended by writing EightBall routines
* When in doubt, do it in a similar way to C!

EightBall resembles an interpreted C.

## Supported Systems
The following 6502-based systems are currently supported:
* Apple II - EightBall runs under ProDOS and uses upper/lowercase text.  It should run on 64K Apple IIe, IIc or IIgs.  It can probably run on Apple II/II+ with an 80 column code, but this has not been tested.
* Commodore 64 - EightBall should run on any C64.
* Commodore VIC-20 - EightBall runs on a VIC-20 with 32K of additional RAM.

## Getting Started
There are executables and disk images available to download for Apple II, Commodore 64 and VIC-20.  These may be run on real hardware or one of the many emulators that are available.

The language itself is documented in these wiki pages.  The best way to learn is to study example programs.

Disk images:
- Test.dsk - ProDOS 2.4.1 bootable disk with EightBall for Apple IIe Enhanced, //c, IIgs.
- test.d64 - Commodore 1541 disk images with EightBall for VIC20 and C64.

## Build Toolchain
I am building EightBall using cc65 v2.15 on Ubuntu Linux.  Please let me know if you need help with compilation.

The Linux version of EightBall is currently being built using gcc 6.3.0.

# EightBall Language Reference and Tutorial

## Variables

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
    
## Expressions

## Constants
Constants may be decimal:

    byte a = 10
    word w = 65535
    word q = -1

or hex:

    byte a = $0a
    word w = $face

## Operators
EightBall supports most of C's arithmetic, logical and bitwise operators.  They have the same precedence as in C as well.  Since the Commodore machines do not have all the ASCII character, some substitutions have been made (shown in parenthesis below.)

EightBall also implements 'star operators' for pointer dereferencing which will also be familiar to C programmers.

### Arithmetic
* Addition: binary `+`
* Subtraction: binary `-`
* Multiplication: binary `*`
* Division: binary `/`
* Modulus: binary `%`
* Power: binary `^`
* Negation: unary prefix `-`

### Logical
* Logical equality: binary `==`
* Logical inequality: binary `!=`
* Logical greater-than: binary `>`
* Logical greater-than-or-equal: binary `>=`
* Logical less-than: binary `<`
* Logical less-than-or-equal: binary `<=`
* Logical and: binary `&&`
* Logical or: binary `||` (binary `##` on CBM)
* Logical not: unary `!`

### Bitwise
* Bitwise and: binary `&`
* Bitwise or: binary `|` (binary `#` on CBM)
* Bitwise xor: binary `!`
* Left shift: binary `<<`
* Right shift: binary `>>`
* Bitwise not: unary prefix `~` (unary prefix `.` on CBM)

### Address-of Operator
The `&` prefix operator returns a pointer to a variable which may be used to read and write the variable's contents.  The operator may be applied to scalar variables, whole arrays and individual elements of arrays.

    word w = 123;
    word A[10] = 0;
    pr.dec &w;       ' Address of scalar w
    pr.dec &A;       ' Address of start of array A
    pr.dec &A[2]     ' Address of third element of array A

### 'Star Operators'
EightBall provides two 'star operators' which dereference pointers in a manner similar to the C star operator.  One of these (`*`) operates on word values, the other (`^`) operates on byte values.  Each of the operators may be used both for reading and writing through pointers.

Here is an example of a pointer to a word value:

    word val = 0;     ' Real value stored here
    word addr = &val; ' Now addr points to val
    *addr = 123;      ' Now val is 123
    pr.dec *addr;     ' Recover the value via the pointer
    pr.nl

Here is an example using a pointer to byte.  This is similar to `PEEK` and `POKE` in BASIC.

    word addr = $c000; ' addr points to hex $c000
    byte val = ^addr;  ' Read value from $c000 (PEEK)
    ^val = 0;          ' Set value at $c000 to zero (POKE)

### Parenthesis
Parenthesis may be used to control the order of evaluation, for example:

    pr.dec (10+2)*3;   ' Prints 36
    pr.dec 10+2*3;     ' Prints 16

### Operator Precedence

| Precedence Level | Operators          | Example  | Example CBM |
| ---------------- | ------------------ | -------- | ----------- |
| 11 (Highest)     | Prefix Plus        | +a       |             |
|                  | Prefix Minus       | -a       |             |
|                  | Prefix Star        | *a       |             |
|                  | Prefix Caret       | ^a       |             |
|                  | Prefix Logical Not | !a       |             |
|                  | Prefix Bitwise Not | ~a       | .a          |
| 10               | Power of           | a ^ b    |             |
|                  | Divide             | a / b    |             |
|                  | Multiply           | a * b    |             |
|                  | Modulus            | a % b    |             |
| 9                | Add                | a + b    |             |
|                  | Subtract           | a - b    |             |
| 8                | Left Shift         | a << b   |             |
|                  | Right Shift        | a >> b   |             |
| 7                | Greater Than       | a > b    |             |
|                  | Greater Than Equal | a >= b   |             |
|                  | Less Than          | a < b    |             |
|                  | Less Than Equal    | a <= b   |             |
| 6                | Equality           | a == b   |             |
|                  | Inequality         | a != b   |             |
| 5                | Bitwise And        | a & b    |             |
| 4                | Bitwise Xor        | a ! b    |             |
| 3                | Bitwise Or         | a \|  b  | a # b       |
| 2                | Logical And        | a && b   |             |
| 1 (Lowest)       | Logical Or         | a \|\| b | a ## b      |


## Flow Control

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

## Subroutines

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

## Code Format

### Whitespace, Semicolon Separators

EightBall code can be arranged however you wish.  For example, this:

    word w = 0; for w = 1 : 10; pr.dec w; pr.nl; endfor

is identical to this:

    word w = 0
    for w = 1 : 10
      pr.dec w; pr.nl
    endfor

Semicolons **must** be used to separate multiple statements on a line (even loop contructs as seen in the first example above.)

Indentation of the code (as shown in the examples in this Wiki) is optional, but encouraged.

### Comments

Comments are introduced by the single quote character.  A full line comment may be entered as follows:

    ' This is a comment

If you wish to comment after a statement, note that a semicolon is required to separate the statement and the comment:

    pr.msg "Hello there"; ' Say hello!!!

## Bits and Pieces

### Run Stored Program
Simple:

    run

Program runs until it hits an `end` statement, an error occurs or it is interrupted by the user.

### Compile Stored Program

    comp

The program in memory is compiled to EightBall VM bytecode.  This is written to a file `bytecode`.

The `bytecode` file may be executed using the EightBall Virtual Machine that is part of this package.

### Quit EightBall
Easy:

    quit

Returns to ProDOS on Apple II, or to CBM BASIC on C64/VIC20.

### Clear Stored Program

    new

### Clear All Variables

    clear

### Show All Variables

    vars

Variables are shown in tabular form.  The letter 'b' indicates byte type, while 'w' indicates word type.  For scalar variables, the value is shown.  For arrays, the dimension(s) are shown.

### Show Free Space

    free

The free space available for variables and for program text is shown on the console.

## Input and Output

Only console I/O is supported at present.  File I/O is planned for a later release.

### Console Output

#### pr.msg
Prints a literal string to the console:

    pr.msg "Hello world"

#### pr.dec
Prints an unsigned decimal value to the console:

    pr.dec 123/10

#### pr.dec.s
Prints a signed decimal value to the console:

    pr.dec.s 12-101

#### pr.hex
Prints a hexadecimal value to the console (prefixed with '$'):

    pr.hex 1234

#### pr.nl
Prints a newline to the console:

    pr.nl

#### pr.str
Prints a byte array as a string to the console.  The string is null terminated (so printing stops at the first 0 character):

    pr.str A; ' A is a byte array

### Console Input

#### kbd.ch
Allows a single character to be read from the keyboard.  Be careful - this function assumes the argument passed to it a pointer to a byte value into which the character may be stored.

    byte c = 0
    while 1
      kbd.ch &c
      pr.ch c
    endwhile

#### kbd.ln
Allows a line of input to be read from the keyboard and to be stored to an array of byte values.  This statement takes two arguments - the first is an array of byte values into which to write the string, the second is the maximum number of bytes to write.

    byte buffer[100] = 0;
    kbd.ln buffer, 100
    pr.msg "You typed> "
    pr.str buffer
    pr.nl

## Line Editor
Eightball includes a simple line editor for editing program text.  Programs are saved to disk in plain text format (ASCII on Apple II, PETSCII on CBM).

Be warned that the line editor is rather primitive.  However we are trying to save memory.

Editor commands start with the colon character (:).

### Load from Disk
To load a new source file from disk, use the `:r` 'read' command:

    :r "myfile.8b"

### Save to Disk
To save the current editor buffer to disk, use the :w 'write' command:

    :w "myfile.8b"

On Commodore systems, this must be a new (non-existing) file, or a drive error will result.

### Insert Line(s)

Start inserting text before the specified line.  The editor switches to insert mode, indicated by the '>' character (in inverse green on CBM).  The following command will start inserting text at the beginning of an empty buffer:

    :i0
    >

One or more lines of code may then be entered.  When you are done, enter a period '.' on a line on its own to return to EightBall immediate mode prompt.

### Append Line(s)

Append is identical to the insert command described above, except that it starts inserting /after/ the specified line.  This is often useful to adding lines following the end of an existing program.

### Delete Line(s)

This command allows one or more lines to be deleted.  To delete one line:

    :d33

or to delete a range of lines:

    :d10,12

### Change Line

This command allows an individual line to be replaced (like inserting a new line the deleting the old line).  It is different to the insert and append commands in that the text is entered immediately following the command (not on a new line).  For example:

    :c21:word var1=12

will replace line 21 with `word var1=12`.  Note the colon terminator following the line number.

Note that the syntax of this command is contrived to allow the CBM screen editor to work on listed output in a similar way to CBM BASIC.  Code may be listed using the `:l` command and the screen may then be interactively edited using the cursor keys and return, just as in BASIC.

### List Line(s)

This allows the program text to be listed to the console.  Either the whole program may be displayed or just a range of lines.  To show everything:

    :l

To show a range of lines:

    :l0-20

(The command is the letter Ell, not the number 1!)

# EightBall Compiler and Virtual Machine

## What is it?

The EightBall Virtual Machine is a simple runtime VM for executing the bytecode produced by the EightBall compiler.  The EightBall VM can run on 6502 systems (Apple II, Commodore VIC20, C64) or as a Linux process.

## How to use it?

The EightBall system is split into two separate executables:
- EightBall editor, interpreter and compiler 
- EightBall VM, which runs the code built by the compiler

On Linux, the editor/interpreter/compiler is `eightball` and the Virtual Machine is `eightballvm`.

On Apple II ProDOS, the editor/interpreter/compiler is `eightball.system` and the VM is `8bvm.system`.

On Commodore VIC20, the editor/interpreter/compiler is `8ball20.prg` and the VM is `8ballvm20.prg`.

On Commodore C64, the editor/interpreter/compiler is `8ball64.prg` and the VM is `8ballvm64.prg`.

Here is how to use the compiler:
- Start the main EightBall editor/interpreter/compiler program.
- Write your program in the editor.
- Debug using the interpreter (`run` command).
- When it seems to work work okay, you can compile with the `comp` command.

The compiler will dump an assembly-style listing to the console and also write the VM bytecode to a binary file called `bytecode`.  If all goes well, no inscrutable error messages will be displayed.

Then you can run the VM program for your platform.  It will load the bytecode from the file `bytecode` and execute it.  Running compiled code under the Virtual Machine is much faster than the interpreter (and also more memory efficient.)

## VM Internals

# Code Examples

## Recursive Factorial

This example shows how EightBall can support recursion.  I should point out that it is much better to do this kind of thing using iteration, but this is a fun simple example:

    pr.dec fact(3); pr.nl
    end

    sub fact(word val)
      pr.msg "fact("; pr.dec val; pr.msg ")"; pr.nl
      if val == 0
        return 1
      else
        return val * fact(val-1)
      endif
    endsub

`fact(3)` calls `fact(2)`, which calls `fact(1)`, then finally `fact(0)`.

See `eightballvm.h` for technical details.

## Prime Number Sieve

Here is the well-known Sieve of Eratosthenes algorithm for finding prime numbers, written in EightBall:

    ' Sieve of Eratosthenes                                                         
                                                                                
    ' Globals                                                                       
    byte nr = 10                                                                    
    word n = nr * nr                                                                
    byte A[100] = 1 ' Has to be a literal                                                                   
                                                                                
    pr.msg "Sieve of Eratosthenes ..."; pr.nl                                       
    call doall()                                                                    
    end                                                                             
                                                                                
    sub doall()                                                                     
      call sieve()                                                                  
      call printresults()                                                           
    endsub                                                                       
                                                                                
    sub sieve()                                                                     
      word i = 0; word j = 0                                                        
      for i = 2 : (nr - 1)                                                          
        if A[i]                                                                     
          j = i * i                                                                 
          while (j < n)                                                             
            A[j] = 0                                                                
            j = j + i                                                               
          endwhile                                                                  
        endif                                                                       
      endfor                                                                        
    endsub                                                                        
                                                                                
    sub printresults()                                                              
      word i = 0                                                                    
      for i = 2 : (n - 1)                                                           
        if A[i]                                                                     
          if i > 2                                                                  
            pr.msg ", "                                                             
          endif                                                                     
          pr.dec i                                                                  
        endif                                                                       
      endfor                                                                        
      pr.msg "."                                                                    
    endsub                                                                        
                                                                                
  
