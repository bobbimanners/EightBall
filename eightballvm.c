/**************************************************************************/
/* EightBall Virtual Machine                                              */
/*                                                                        */
/* The Eight Bit Algorithmic Language                                     */
/* For Apple IIe/c/gs (64K), Commodore 64, VIC-20 +32K RAM expansion      */
/* (also builds for Linux as 32 bit executable (gcc -m32) only)           */
/*                                                                        */
/* Compiles with cc65 v2.15 for VIC-20, C64, Apple II                     */
/* and gcc 7.3 for Linux                                                  */
/*                                                                        */
/* Note that this code assumes that sizeof(int) = sizeof(int*), which is  */
/* true for 6502 (16 bits each) and i686 (32 bits each) - but not amd64   */
/*                                                                        */
/* cc65: Define symbol VIC20 to build for Commodore VIC-20 + 32K.         */
/*       Define symbol C64 to build for Commodore 64.                     */
/*       Define symbol A2E to build for Apple //e.                        */
/*                                                                        */
/* Copyright Bobbi Webber-Manners 2018                                    */
/* Reference implementation of EightBall Virtual Machine.                 */
/*                                                                        */
/* This is not intended to be optimized for speed.  I plan to implement   */
/* an optimized version in 6502 assembler later.                          */
/*                                                                        */
/* Formatted with indent -kr -nut                                         */
/**************************************************************************/

/**************************************************************************/
/*  GNU PUBLIC LICENCE v3 OR LATER                                        */
/*                                                                        */
/*  This program is free software: you can redistribute it and/or modify  */
/*  it under the terms of the GNU General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or     */
/*  (at your option) any later version.                                   */
/*                                                                        */
/*  This program is distributed in the hope that it will be useful,       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/*  GNU General Public License for more details.                          */
/*                                                                        */
/*  You should have received a copy of the GNU General Public License     */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/*                                                                        */
/**************************************************************************/

/*
#define DEBUG
#define EXTRADEBUG
#define DEBUGREGS
#define DEBUGADDRESSING
#define DEBUGSTACK
*/

/* Define STACKCHECKS to enable paranoid stack checking */
#ifdef __GNUC__
#define STACKCHECKS
#else
#undef STACKCHECKS
#endif

#include "eightballvm.h"
#include "eightballutils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef A2E
#include <conio.h>
#endif

#define EVALSTACKSZ  16

/*
 * Call stack grows down from top of memory.
 * If it hits CALLSTACKLIM then VM will quit with error
 */
#ifdef __GNUC__
#define MEMORYSZ     (64 * 1024)
#else
  /* Has to be 64K minus a byte otherwise winds up being zero! */
#define MEMORYSZ     (64 * 1024) - 1
#endif
#define CALLSTACKLIM (32 * 1024)

#ifdef __GNUC__
#define UINT16 unsigned short
#else
#define UINT16 unsigned short
#endif

#ifndef A2E

unsigned char evalptr;          /* Points to the empty slot above top of eval stack */
UINT16 pc;                      /* Program counter */
UINT16 sp;                      /* Stack pointer   */
UINT16 fp;                      /* Frame pointer   */
unsigned short *wordptr;        /* Temp used in execute() */
unsigned char *byteptr;         /* Temp used in execute() */
UINT16 evalstack[EVALSTACKSZ];  /* Evaluation stack - 16 bit ints.  Addressed by evalptr */

#else

/*
 * Zero page variables used on Apple II only at present
 */

extern unsigned char evalptr;
extern UINT16 pc;
extern UINT16 sp;
extern UINT16 fp;
extern unsigned short *wordptr;
extern unsigned char *byteptr;
extern UINT16 evalstack[EVALSTACKSZ];

#pragma zpsym ("evalptr");
#pragma zpsym ("pc");
#pragma zpsym ("sp");
#pragma zpsym ("fp");
#pragma zpsym ("wordptr");
#pragma zpsym ("byteptr");
#pragma zpsym ("evalstack");

#endif

/*
 * System memory - addressed in bytes.
 * Used for program storage.  Addressed by pc.
 *  - Programs are stored from address 0 upwards.
 * Used for callstack.  Addressed by sp.
 *  - Callstack grows down from top of memory.
 */
#ifdef __GNUC__
unsigned char memory[MEMORYSZ];
#else
unsigned char *memory = 0;
#endif

/*
 * Macro to ensure efficient code generation on cc65 when accessing memory
 */
#ifdef __GNUC__
#define MEM(x) memory[x]
#else
#define MEM(x) (*(unsigned char*)x)
#endif

#define XREG evalstack[evalptr - 1]     /* Only valid if evalptr >= 1 */
#define YREG evalstack[evalptr - 2]     /* Only valid if evalptr >= 2 */
#define ZREG evalstack[evalptr - 3]     /* Only valid if evalptr >= 3 */
#define TREG evalstack[evalptr - 4]     /* Only valid if evalptr >= 4 */

/*
 * Error checks are called through macros to make it easy to
 * disable them in production.  We should not need these checks
 * in production (assuming no bugs in the compiler!) ... but they
 * are helpful for debugging!
 */

#ifdef STACKCHECKS

/* Check evaluation stack is not going to underflow */
#define CHECKUNDERFLOW(level) checkunderflow(level)

/* Check evaluation stack is not going to overflow */
#define CHECKOVERFLOW() checkoverflow()

/* Check call stack is not going to underflow */
#define CHECKSTACKUNDERFLOW(bytes) checkstackunderflow(bytes)

/* Check call stack is not going to overflow */
#define CHECKSTACKOVERFLOW(bytes) checkstackoverflow(bytes)

#else

/* For production use, do not do these checks */
#define CHECKUNDERFLOW(level)
#define CHECKOVERFLOW()
#define CHECKSTACKUNDERFLOW(bytes)
#define CHECKSTACKOVERFLOW(bytes)
#endif

/* Handler for unsupported bytecode */
#define UNSUPPORTED() unsupported()

#ifdef STACKCHECKS
/*
 * Check for evaluation stack underflow.
 * level - Number of 16 bit operands required on eval stack.
 */
void checkunderflow(unsigned char level)
{
    if (evalptr < level) {
        print("Eval stack underflow\nPC=");
        printhex(pc);
        printchar('\n');
        while (1);
    }
}

/*
 * Check evaluation stack is not going to overflow.
 * Assumes evalptr has already been advanced.
 */
void checkoverflow()
{
    if (evalptr > EVALSTACKSZ - 1) {
        print("Eval stack overflow\nPC=");
        printhex(pc);
        printchar('\n');
        while (1);
    }
}

/*
 * Check call stack is not going to underflow.
 * bytes - Number of bytes required on call stack.
 */
void checkstackunderflow(unsigned char bytes)
{
    if ((MEMORYSZ - sp) < bytes) {
        print("Call stack underflow\nPC=");
        printhex(pc);
        printchar('\n');
        while (1);
    }
}

/*
 * Check call stack is not going to overflow.
 * Assumes sp has already been advanced.
 */
void checkstackoverflow()
{
    if (sp < CALLSTACKLIM + 1) {
        print("Call stack overflow\nPC=");
        printhex(pc);
        printchar('\n');
        while (1);
    }
}
#endif

/*
 * Handler for unsupported bytecodes
 */
void unsupported()
{
    print("Unsupported instruction ");
    printhexbyte(MEM(pc));
    print("\nPC=");
    printhex(pc);
    printchar('\n');
    while (1);
}

/*
 * Fetch, decode and execute a VM instruction.
 * Advance program counter and loop until VM_END.
 */
void execute()
{
    unsigned short d;
#ifdef DEBUGREGS
    unsigned short i;
#endif

    evalptr = 0;
    pc = RTPCSTART;
    sp = fp = RTCALLSTACKTOP;

    while (1) {

nextinstr:

    //print("--->PC "); printhex(pc); print(" eval stk: "); printhex(evalptr); print("\n");
#ifdef DEBUGREGS
    print("\n");
    print("--->PC ");
    printhex(pc);
    print("\n");
    print("--->SP ");
    printhex(sp);
    print("\n");
    print("--->FP ");
    printhex(fp);
    print("\n");
    print("Call Stk: ");
    for (i = sp + 1; i <= RTCALLSTACKTOP; ++i) {
        printhexbyte(MEM(i));
        printchar(' ');
    }
    print("\nEval Stk: ");
    printhex(XREG);
    printchar(' ');
    printhex(YREG);
    printchar(' ');
    printhex(ZREG);
    printchar(' ');
    printhex(TREG);
    printchar('\n');
#endif

#ifdef DEBUG
#ifdef A2E
    printchar('\r');
#else
    printchar('\n');
#endif
    printhex(pc);
    print(": ");
    print(bytecodenames[MEM(pc)]);
/* TODO: Should encode immediate mode as one bit of opcode to make this more efficient */
    if ((MEM(pc) == VM_LDIMM) ||
        (MEM(pc) == VM_LDAWORDIMM) ||
        (MEM(pc) == VM_LDABYTEIMM) ||
        (MEM(pc) == VM_LDRWORDIMM) ||
        (MEM(pc) == VM_LDRBYTEIMM) ||
        (MEM(pc) == VM_STAWORDIMM) ||
        (MEM(pc) == VM_STABYTEIMM) ||
        (MEM(pc) == VM_STRWORDIMM) ||
        (MEM(pc) == VM_STRBYTEIMM) ||
        (MEM(pc) == VM_JMPIMM) ||
        (MEM(pc) == VM_BRNCHIMM) ||
        (MEM(pc) == VM_JSRIMM)) {
        printchar(' ');
        wordptr = (unsigned short *)&MEM(pc + 1);
        printhex(*wordptr);
        printchar(' ');
    } else {
        print("       ");
    }
#ifdef EXTRADEBUG
    print("stk: ");
    if (evalptr >= 1) {
        printhex(XREG);
    }
    if (evalptr >= 2) {
        print(", ");
        printhex(YREG);
    }
    if (evalptr >= 3) {
        print(", ");
        printhex(ZREG);
    }
    if (evalptr >= 4) {
        print(", ");
        printhex(TREG);
    }
    if (evalptr >= 5) {
        print(", ");
        printhex(evalstack[evalptr - 5]);
    }
    if (evalptr >= 6) {
        print(" ...");
    }
#endif
#endif

    switch (MEM(pc)) {
        /*
         * Miscellaneous
         */
    case VM_END:               /* Terminate execution                                          */
        if (evalptr > 0) {
            print("WARNING: evalptr ");
            printdec(evalptr);
            printchar('\n');
        }
#ifdef __GNUC__
        exit(0);
#else
        for (d = 0; d < 25000; ++d);
        exit(0);
#endif
        break;

        /*
         * Load Immediate
         */
    case VM_LDIMM:             /* Pushes the following 16 bit word to the evaluation stack     */
        ++evalptr;
        CHECKOVERFLOW();
        wordptr = (unsigned short *)&MEM(++pc);
        ++pc;
        XREG = *wordptr;
        break;
        /*
         * Absolute addressing:
         * XREG points to absolute address within system memory.
         */
    case VM_LDAWORD:           /* Replaces X with 16 bit value pointed to by X.                */
        CHECKUNDERFLOW(1);

#ifdef DEBUGADDRESSING
        print("\n  XREG: ");
        printhex(XREG);
        printchar('\n');
#endif

        wordptr = (unsigned short *)&MEM(XREG);
        XREG = *wordptr;
        break;
    case VM_LDAWORDIMM:        /* Imm mode - push 16 bit value pointed to by addr after opcode */
        ++evalptr;
        CHECKOVERFLOW();
        wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
        wordptr = (unsigned short *)&MEM(*wordptr); /* Pointer to variable */
        ++pc;
        XREG = *wordptr;
        break;
    case VM_LDABYTE:           /* Replaces X with 8 bit value pointed to by X.                 */
        CHECKUNDERFLOW(1);
        XREG = MEM(XREG);
        break;
    case VM_LDABYTEIMM:        /* Imm mode - push byte pointed to by addr after opcode         */
        ++evalptr;
        CHECKOVERFLOW();
        wordptr = (unsigned short *)&MEM(++pc);    /* Pointer to operand */
        byteptr = (unsigned char *)&MEM(*wordptr); /* Pointer to variable */
        ++pc;
        XREG = *byteptr;
        break;
    case VM_STAWORD:           /* Stores 16 bit value Y in addr pointed to by X. Drops X and Y.*/
        CHECKUNDERFLOW(2);
        wordptr = (unsigned short *)&MEM(XREG);
        *wordptr = YREG;
        evalptr -= 2;
        break;
    case VM_STAWORDIMM:        /* Imm mode - store 16 bit value X in addr after opcode. Drop X.*/
        CHECKUNDERFLOW(1);
        wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
        wordptr = (unsigned short *)&MEM(*wordptr); /* Pointer to variable */
        ++pc;
        *wordptr = XREG;
        --evalptr;
        break;
    case VM_STABYTE:           /* Stores 8 bit value Y in addr pointed to by X. Drops X and Y. */
        CHECKUNDERFLOW(2);
        MEM(XREG) = YREG;
        evalptr -= 2;
        break;
    case VM_STABYTEIMM:        /* Imm mode - store 8 bit value X in addr after opcode. Drop X.*/
        CHECKUNDERFLOW(1);
        wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
        byteptr = (unsigned char *)&MEM(*wordptr);  /* Pointer to variable */
        ++pc;
        *byteptr = XREG;
        --evalptr;
        break;
        /*
         * Relative to Frame Pointer addressing:
         * XREG points to address in system memory relative to the frame pointer.
         */
    case VM_LDRWORD:           /* Replaces X with 16 bit value pointed to by X.                */
        CHECKUNDERFLOW(1);

#ifdef DEBUGADDRESSING
        print("\n  XREG: ");
        printhex(XREG);
        print(", FP: ");
        printhex(fp);
        print(" -> ");
        printhex((XREG + fp + 1) & 0xffff);
        printchar('\n');
#endif

#ifdef __GNUC__
        wordptr = (unsigned short *)&memory[(XREG + fp + 1) & 0xffff];
#else
        wordptr = (unsigned short *)&memory[XREG + fp + 1];
#endif
        XREG = *wordptr;
        break;
    case VM_LDRWORDIMM:        /* Imm mode - push 16 bit value pointed to by addr after opcode */
        ++evalptr;
        CHECKOVERFLOW();
        wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
#ifdef __GNUC__
        wordptr = (unsigned short *)&memory[(*wordptr + fp + 1) & 0xffff]; /* Pointer to variable */
#else
        wordptr = (unsigned short *)&memory[*wordptr + fp + 1]; /* Pointer to variable */
#endif
        ++pc;
        XREG = *wordptr;
        break;
    case VM_LDRBYTE:           /* Replaces X with 8 bit value pointed to by X.                 */
        CHECKUNDERFLOW(1);

#ifdef DEBUGADDRESSING
        print("\n  XREG: ");
        printhex(XREG);
        print(", FP: ");
        printhex(fp);
        print(" -> ");
        printhex((XREG + fp + 1) & 0xffff);
        printchar('\n');
#endif

#ifdef __GNUC__
        XREG = MEM((XREG + fp + 1) & 0xffff);
#else
        XREG = MEM(XREG + fp + 1);
#endif
        break;
    case VM_LDRBYTEIMM:        /* Imm mode - push byte pointed to by addr after opcode          */
        ++evalptr;
        CHECKOVERFLOW();
        wordptr = (unsigned short *)&MEM(++pc);    /* Pointer to operand */
#ifdef __GNUC__
        byteptr = (unsigned char *)&memory[(*wordptr + fp + 1) & 0xffff]; /* Pointer to variable */
#else
        byteptr = (unsigned char *)&memory[*wordptr + fp + 1]; /* Pointer to variable */
#endif
        ++pc;
        XREG = *byteptr;
        break;
    case VM_STRWORD:           /* Stores 16 bit value Y in addr pointed to by X. Drops X and Y. */
        CHECKUNDERFLOW(2);
#ifdef __GNUC__
        wordptr = (unsigned short *)&memory[(XREG + fp + 1) & 0xffff];
#else
        wordptr = (unsigned short *)&memory[XREG + fp + 1];
#endif
        *wordptr = YREG;
        evalptr -= 2;
        break;
    case VM_STRWORDIMM:        /* Imm mode - store 16 bit value X in addr after opcode. Drop X.*/
        CHECKUNDERFLOW(1);
        wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
#ifdef __GNUC__
        wordptr = (unsigned short *)&memory[(*wordptr + fp + 1) & 0xffff]; /* Pointer to variable */
#else
        wordptr = (unsigned short *)&memory[*wordptr + fp + 1]; /* Pointer to variable */
#endif
        ++pc;
        *wordptr = XREG;
        --evalptr;
        break;
    case VM_STRBYTE:           /* Stores 8 bit value Y in addr pointed to by X. Drops X and Y. */
        CHECKUNDERFLOW(2);
#ifdef __GNUC__
        memory[(XREG + fp + 1) & 0xffff] = YREG;
#else
        memory[XREG + fp + 1] = YREG;
#endif
        evalptr -= 2;
        break;
    case VM_STRBYTEIMM:        /* Imm mode - store 8 bit value X in addr after opcode. Drop X.*/
        CHECKUNDERFLOW(1);
        wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
#ifdef __GNUC__
        byteptr = (unsigned char *)&memory[(*wordptr + fp + 1) & 0xffff];  /* Pointer to variable */
#else
        byteptr = (unsigned char *)&memory[*wordptr + fp + 1];  /* Pointer to variable */
#endif
        ++pc;
        *byteptr = XREG;
        --evalptr;
        break;
        /*
         * Manipulate evaluation stack
         */
    case VM_SWAP:              /* Swaps X and Y                                                */
        CHECKUNDERFLOW(2);
        d = XREG;
        XREG = YREG;
        YREG = d;
        break;
    case VM_DUP:               /* Duplicates X -> X, Y                                         */
        CHECKUNDERFLOW(1);
        ++evalptr;
        CHECKOVERFLOW();
        XREG = YREG;
        break;
    case VM_DUP2:              /* Duplicates X -> X,Z; Y -> Y,T                                */
        CHECKUNDERFLOW(2);
        evalptr += 2;
        CHECKOVERFLOW();
        XREG = ZREG;
        YREG = TREG;
        break;
    case VM_DROP:              /* Drops X                                                      */
        CHECKUNDERFLOW(1);
        --evalptr;
        break;
    case VM_OVER:              /* Duplicates Y -> X,Z                                           */
        CHECKUNDERFLOW(2);
        ++evalptr;
        CHECKOVERFLOW();
        XREG = ZREG;
        break;
    case VM_PICK:              /* Duplicates stack level specified in X+1  -> X                  */
        CHECKUNDERFLOW(XREG + 1);
        XREG = evalstack[evalptr - (XREG + 1)];
        break;
        /*
         * Manipulate call stack
         */
    case VM_POPWORD:           /* Pop 16 bit value from call stack, push onto eval stack [X]   */
        CHECKSTACKUNDERFLOW(2);
        sp += 2;
        ++evalptr;
        CHECKOVERFLOW();
        wordptr = (unsigned short *)&memory[sp - 1];
        XREG = *wordptr;
        break;
    case VM_POPBYTE:           /* Pop 8 bit value from call stack, push onto eval stack [X]    */
        CHECKSTACKUNDERFLOW(1);
        ++sp;
        ++evalptr;
        CHECKOVERFLOW();
        XREG = MEM(sp);
        break;
    case VM_PSHWORD:           /* Push 16 bit value in X onto call stack.  Drop X.             */
        CHECKUNDERFLOW(1);

#ifdef DEBUGSTACK
        print("\n  Push word to ");
        printhex(sp - 1);
        printchar('\n');
#endif
        byteptr = (unsigned char *)&XREG;
        MEM(sp--) = *(byteptr + 1);
        CHECKSTACKOVERFLOW();
        MEM(sp--) = *byteptr;
        CHECKSTACKOVERFLOW();
        --evalptr;
        break;
    case VM_PSHBYTE:           /* Push 8 bit value in X onto call stack.  Drop X.              */
        CHECKUNDERFLOW(1);

#ifdef DEBUGSTACK
        print("\n  Push byte to ");
        printhex(sp);
        printchar('\n');
#endif

        MEM(sp--) = XREG & 0x00ff;
        CHECKSTACKOVERFLOW();
        --evalptr;
        break;
    case VM_DISCARD:           /* Discard X bytes from call stack.  Drop X.                    */
        CHECKUNDERFLOW(1);
        sp += XREG;
        --evalptr;
        break;
    case VM_SPTOFP:            /* Copy stack pointer to frame pointer. (Enter function scope)  */

#ifdef DEBUGSTACK
        print("\n  SPTOFP  FP before ");
        printhex(fp);
        print(" SP ");
        printhex(sp);
        printchar('\n');
#endif

        /* Push old FP to stack */
        byteptr = (unsigned char *)&fp;
        MEM(sp--) = *(byteptr + 1);
        CHECKSTACKOVERFLOW();
        MEM(sp--) = *byteptr;
        CHECKSTACKOVERFLOW();

        fp = sp;
        break;
    case VM_FPTOSP:            /* Copy frame pointer to stack pointer. (Release local vars)    */

#ifdef DEBUGSTACK
        print("\n  FPTOSP  SP before ");
        printhex(sp);
        print(" FP ");
        printhex(fp);
        printchar('\n');
#endif

        sp = fp;

        /* Pop old FP from stack -> FP */
        CHECKSTACKUNDERFLOW(2);
        sp += 2;
        CHECKOVERFLOW();
        wordptr = (unsigned short *)&memory[sp - 1];
        fp = *wordptr;

#ifdef DEBUGSTACK
        print("  Recovered FP ");
        printhex(fp);
        print(" from stack\n");
#endif

        break;
    case VM_ATOR:              /* Convert absolute address in X to relative address            */
#ifdef __GNUC__
        XREG = (XREG - fp - 1) & 0xffff;
#else
        XREG = XREG - fp - 1;
#endif
        break;
    case VM_RTOA:              /* Convert relative address in X to absolute address            */
#ifdef __GNUC__
        XREG = (XREG + fp + 1) & 0xffff;
#else
        XREG = XREG + fp + 1;
#endif
        break;
        /*
         * Integer math
         */
    case VM_INC:               /* X = X+1.                                                     */
        CHECKUNDERFLOW(1);
        ++XREG;
        break;
    case VM_DEC:               /* X = X-1.                                                     */
        CHECKUNDERFLOW(1);
        --XREG;
        break;
    case VM_ADD:               /* X = Y+X.  Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG + XREG;
        --evalptr;
        break;
    case VM_SUB:               /* X = Y-X.  Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG - XREG;
        --evalptr;
        break;
    case VM_MUL:               /* X = Y*X.  Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG * XREG;
        --evalptr;
        break;
    case VM_DIV:               /* X = Y/X.  Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG / XREG;
        --evalptr;
        break;
    case VM_MOD:               /* X = Y%X.  Y is dropped                                     . */
        CHECKUNDERFLOW(2);
        YREG = YREG % XREG;
        --evalptr;
        break;
    case VM_NEG:               /* X = -X                                                       */
        CHECKUNDERFLOW(1);
        XREG = -XREG;
        break;
        /*
         * Comparisons
         */
    case VM_GT:                /* X = Y>X.  Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG > XREG;
        --evalptr;
        break;
    case VM_GTE:               /* X = Y>=X. Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG >= XREG;
        --evalptr;
        break;
    case VM_LT:                /* X = Y<X.  Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG < XREG;
        --evalptr;
        break;
    case VM_LTE:               /* X = Y<=X. Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG <= XREG;
        --evalptr;
        break;
    case VM_EQL:               /* X = Y==X. Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG == XREG;
        --evalptr;
        break;
    case VM_NEQL:              /* X = Y!=X. Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG != XREG;
        --evalptr;
        break;
        /*
         * Logical operations
         */
    case VM_AND:               /* X = Y&&X. Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG && XREG;
        --evalptr;
        break;
    case VM_OR:                /* X = Y||X. Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG || XREG;
        --evalptr;
        break;
    case VM_NOT:               /* X = !X                                                       */
        CHECKUNDERFLOW(1);
        XREG = !XREG;
        break;
        /*
         * Bitwise operations
         */
    case VM_BITAND:            /* X = Y&X. Y is dropped.                                       */
        CHECKUNDERFLOW(2);
        YREG = YREG & XREG;
        --evalptr;
        break;
    case VM_BITOR:             /* X = Y|X. Y is dropped.                                       */
        CHECKUNDERFLOW(2);
        YREG = YREG | XREG;
        --evalptr;
        break;
    case VM_BITXOR:            /* X = Y^X. Y is dropped.                                       */
        CHECKUNDERFLOW(2);
        YREG = YREG ^ XREG;
        --evalptr;
        break;
    case VM_BITNOT:            /* X = ~X.                                                      */
        CHECKUNDERFLOW(1);
        XREG = ~XREG;
        break;
    case VM_LSH:               /* X = Y<<X. Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG << XREG;
        --evalptr;
        break;
    case VM_RSH:               /* X = Y>>X. Y is dropped.                                      */
        CHECKUNDERFLOW(2);
        YREG = YREG >> XREG;
        --evalptr;
        break;
        /*
         * Flow control
         */
    case VM_JMP:               /* Jump to address X.  Drop X.                                  */
        CHECKUNDERFLOW(1);
        pc = XREG;
        --evalptr;
        goto nextinstr;        /* Do not advance program counter */
    case VM_JMPIMM:            /* Imm mode - jump to 16 bit word following opcode              */
        wordptr = (unsigned short *)&MEM(++pc);
        ++pc;
        pc = *wordptr;
        goto nextinstr;        /* Do not advance program counter */
    case VM_BRNCH:             /* If Y!= 0, jump to address X.  Drop X, Y.                     */
        CHECKUNDERFLOW(2);
        if (YREG) {
            pc = XREG;
        } else {
            ++pc;
        }
        evalptr -= 2;
        goto nextinstr;        /* Do not advance program counter */
    case VM_BRNCHIMM:          /* Imm mode - if X!=0 branch to 16 bit word following opcode    */
        wordptr = (unsigned short *)&MEM(++pc);
        ++pc;
        CHECKUNDERFLOW(1);
        if (XREG) {
            pc = *wordptr;
        } else {
            ++pc;
        }
        --evalptr;
        goto nextinstr;        /* Do not advance program counter */
    case VM_JSR:               /* Push PC to call stack.  Jump to address X.  Drop X.          */
        CHECKUNDERFLOW(1);
        byteptr = (unsigned char *) &pc;
        MEM(sp) = *(byteptr + 1);
        --sp;
        CHECKSTACKOVERFLOW();
        MEM(sp) = *byteptr;
        --sp;
        CHECKSTACKOVERFLOW();
        pc = XREG;
        --evalptr;
        goto nextinstr;        /* Do not advance program counter */
    case VM_JSRIMM:            /* Imm mode - push PC to calls stack, jump to 16 bit word       */
        wordptr = (unsigned short *)&MEM(++pc);
        ++pc;
        byteptr = (unsigned char *) &pc;
        MEM(sp) = *(byteptr + 1);
        --sp;
        CHECKSTACKOVERFLOW();
        MEM(sp) = *byteptr;
        --sp;
        CHECKSTACKOVERFLOW();
        pc = *wordptr;
        goto nextinstr;        /* Do not advance program counter */
    case VM_RTS:               /* Pop call stack, jump to the address popped.                  */
        CHECKSTACKUNDERFLOW(2);
        ++sp;
        wordptr = (unsigned short *)&MEM(sp);
        pc = *wordptr;
        ++sp;
        break;
        /*
         * Input / Output
         */
    case VM_PRDEC:             /* Print 16 bit decimal in X.  Drop X                           */
        CHECKUNDERFLOW(1);
        printdec(XREG);
        --evalptr;
        break;
    case VM_PRHEX:             /* Print 16 bit hex in X.  Drop X                               */
        CHECKUNDERFLOW(1);
        printhex(XREG);
        --evalptr;
        break;
    case VM_PRCH:              /* Print character in X.  Drop X                                */
        CHECKUNDERFLOW(1);
        printchar((unsigned char) XREG);
        --evalptr;
        break;
    case VM_PRSTR:             /* Print null terminated string pointed to by X.  Drop X        */
        CHECKUNDERFLOW(1);
        while (MEM(XREG)) {
            printchar(MEM(XREG++));
        }
        --evalptr;
        break;
    case VM_PRMSG:             /* Print literal string at PC (null terminated)                 */
        ++pc;
        while (MEM(pc)) {
            printchar(MEM(pc++));
        }
        break;
    case VM_KBDCH:             /* Push character from keyboard onto eval stack                 */
        CHECKUNDERFLOW(1);
        ++evalptr;
        /* Loop until we get a keypress */
#ifdef A2E
        while (!(XREG = getkey()));
#elif defined(CBM)
        while (!(*(char *) XREG = cbm_k_getin()));
#else
        /* TODO: Unimplemented in Linux */
        XREG = 0;
#endif
        break;
    case VM_KBDLN:             /* Obtain line from keyboard and write to memory pointed to by  */
        /* Y. X contains the max number of bytes in buf. Drop X, Y.     */
        CHECKUNDERFLOW(2);
        getln((char *) &MEM(YREG), XREG);
        evalptr -= 2;
        break;
        /*
         * Unsupported instruction
         */
    default:
        UNSUPPORTED();
        break;
    }
    ++pc;
    }
};

/*
 * Load bytecode into memory[].
 */
void load()
{
    FILE *fp;
    char ch;
    char *p = (char*)&MEM(RTPCSTART);

    pc = RTPCSTART;
    do {
#ifndef VIC20
        /* TODO: Not sure why getln() is blowing up on VIC20 */
        print("\nBytecode file (CR for default)>");
        getln(p, 15);
#else
        *p = 0;
#endif
        if (strlen(p) == 0) {
            strcpy(p, "bytecode");
        }
        print("Loading '");
        print(p);
        print("'\n");
        fp = fopen(p, "r");
    } while (!fp);
    while (!feof(fp)) {
        ch = fgetc(fp);
        MEM(pc++) = ch;
        /* Print dot for each page */
        if (pc%0xff == 0) {
            printchar('.');
        }
    }
    fclose(fp);
    pc = RTPCSTART;
#ifdef A2E
    printchar(7);
#endif
}

int main()
{
    print("EightBallVM v" VERSIONSTR "\n");
#ifdef STACKCHECKS
    print("[Stack Checks ON]\n");
#endif
    print("(c)Bobbi, 2018\n");
    print("Free Software.\n");
    print("Licenced under GPL.\n\n");

    load();
#ifdef __GNUC__
    print(" Done.\n\n");
#endif
#ifdef A2E
    videomode(VIDEOMODE_80COL);
    clrscr();
#elif defined(CBM)
    printchar(147);             /* Clear */
#endif
    execute();
    return 0;
}
