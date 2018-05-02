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

#include "eightballvm.h"
#include "eightballutils.h"

#include <stdlib.h>
#include <stdio.h>

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
#define UINT16 unsigned int
#endif

UINT16 pc = RTPCSTART;          /* Program counter */
UINT16 sp = RTCALLSTACKTOP;     /* Stack pointer   */
UINT16 fp = RTCALLSTACKTOP;     /* Frame pointer   */

/* Evaluation stack - 16 bit ints.  Addressed by evalptr */
UINT16 evalstack[EVALSTACKSZ];

/* evalptr points to the empty slot above the top of the evaluation stack */
unsigned char evalptr = 0;

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

/* Check evaluation stack is not going to underflow */
#define CHECKUNDERFLOW(level) checkunderflow(level)

/* Check evaluation stack is not going to overflow */
#define CHECKOVERFLOW() checkoverflow()

/* Check call stack is not going to underflow */
#define CHECKSTACKUNDERFLOW(bytes) checkstackunderflow(bytes)

/* Check call stack is not going to overflow */
#define CHECKSTACKOVERFLOW(bytes) checkstackoverflow(bytes)

/* Handler for unsupported bytecode */
#define UNSUPPORTED() unsupported()

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
        printhex(pc); printchar('\n');
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

/*
* Handler for unsupported bytecodes
*/
void unsupported()
{
    print("Unsupported instruction ");
    printhexbyte(memory[pc]);
    print("\nPC=");
    printhex(pc);
    printchar('\n');
    while (1);
}

/*
* Fetch, decode and execute a VM instruction, then advance the program counter.
*/
void execute_instruction()
{
    unsigned int tempword;
    unsigned char *byteptr;
#ifndef __GNUC__
    unsigned int delay;
#endif

#ifdef DEBUGREGS
    unsigned int i;
    print("\n");
    print("--->PC "); printhex(pc); print("\n");
    print("--->SP "); printhex(sp); print("\n");
    print("--->FP "); printhex(fp); print("\n");
    print("Call Stk: ");
    for(i = sp+1; i <= RTCALLSTACKTOP; ++i) {
	printhexbyte(memory[i]); printchar(' ');
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
    print(bytecodenames[memory[pc]]);
    if (memory[pc] == VM_LDIMM) {
        printchar(' ');
        printhex(memory[pc + 1] + 256 * memory[pc + 2]);
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

    switch (memory[pc]) {
    case VM_LDIMM:             /* Pushes the following 16 bit word to the evaluation stack     */
        ++evalptr;
	CHECKOVERFLOW();
        /* Note: Word is stored in little endian format! */
        tempword = memory[++pc];
        tempword += memory[++pc] * 256;
        XREG = tempword;
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

        XREG = memory[XREG] + 256 * memory[XREG + 1];
        break;
    case VM_LDABYTE:           /* Replaces X with 8 bit value pointed to by X.                 */
        CHECKUNDERFLOW(1);
        XREG = memory[XREG];
        break;
    case VM_STAWORD:           /* Stores 16 bit value Y in addr pointed to by X. Drops X and Y. */
        CHECKUNDERFLOW(2);
        memory[XREG] = YREG & 0x00ff;
        memory[XREG + 1] = (YREG & 0xff00) >> 8;
        evalptr -= 2;
        break;
    case VM_STABYTE:           /* Stores 8 bit value Y in addr pointed to by X. Drops X and Y. */
        CHECKUNDERFLOW(2);
        memory[XREG] = YREG;
        evalptr -= 2;
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

        XREG = memory[(XREG + fp + 1) & 0xffff] + 256 * memory[(XREG + fp + 2) & 0xffff];
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

        XREG = memory[(XREG + fp + 1) & 0xffff];
        break;
    case VM_STRWORD:           /* Stores 16 bit value Y in addr pointed to by X. Drops X and Y. */
        CHECKUNDERFLOW(2);
        memory[(XREG + fp + 1) & 0xffff] = YREG & 0x00ff;
        memory[(XREG + fp + 2) & 0xffff] = (YREG & 0xff00) >> 8;
        evalptr -= 2;
        break;
    case VM_STRBYTE:           /* Stores 8 bit value Y in addr pointed to by X. Drops X and Y. */
        CHECKUNDERFLOW(2);
        memory[(XREG + fp + 1) & 0xffff] = YREG;
        evalptr -= 2;
        break;
        /*
         * Manipulate evaluation stack
         */
    case VM_SWAP:              /* Swaps X and Y                                                */
        CHECKUNDERFLOW(2);
        tempword = XREG;
        XREG = YREG;
        YREG = tempword;
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
        XREG = memory[sp - 1] + 256 * memory[sp];
        break;
    case VM_POPBYTE:           /* Pop 8 bit value from call stack, push onto eval stack [X]    */
        CHECKSTACKUNDERFLOW(1);
        ++sp;
        ++evalptr;
        CHECKOVERFLOW();
        XREG = memory[sp];
        break;
    case VM_PSHWORD:           /* Push 16 bit value in X onto call stack.  Drop X.             */

#ifdef DEBUGSTACK
        print("\n  Push word to ");
        printhex(sp-1);
        printchar('\n');
#endif

        memory[sp] = (XREG & 0xff00) >> 8;
        --sp;
        CHECKSTACKOVERFLOW();
        memory[sp] = XREG & 0x00ff;
        --sp;
        CHECKSTACKOVERFLOW();
        --evalptr;
        break;
    case VM_PSHBYTE:           /* Push 8 bit value in X onto call stack.  Drop X.              */

#ifdef DEBUGSTACK
        print("\n  Push byte to ");
        printhex(sp);
        printchar('\n');
#endif

        memory[sp] = XREG & 0x00ff;
        --sp;
        CHECKSTACKOVERFLOW();
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
        memory[sp] = (fp & 0xff00) >> 8;
        --sp;
        CHECKSTACKOVERFLOW();
        memory[sp] = fp & 0x00ff;
        --sp;
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
        fp = memory[sp - 1] + 256 * memory[sp];

#ifdef DEBUGSTACK
        print("  Recovered FP ");
	printhex(fp);
	print(" from stack\n");
#endif

        break;
        /*
         * Miscellaneous
         */
    case VM_RTOA:              /* Convert relative address in X to absolute address            */
        XREG = (XREG + fp + 1) & 0xffff;
	break;
    case VM_END:               /* Terminate execution                                          */
#ifdef __GNUC__
        exit(0);
#else
        /* Spin forever */
        for (delay = 0; delay < 25000; ++delay);
	exit(0);
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
        return;                 /* Do not advance program counter */
    case VM_BRNCH:             /* If Y!= 0, jump to address X.  Drop X, Y.                     */
        CHECKUNDERFLOW(2);
        if (YREG) {
            pc = XREG;
        } else {
            ++pc;
        }
        evalptr -= 2;
        return;                 /* Do not advance program counter */
    case VM_JSR:               /* Push PC to call stack.  Jump to address X.  Drop X.          */
        CHECKUNDERFLOW(1);
        byteptr = (unsigned char *) &pc;
        memory[sp] = *byteptr;
        --sp;
        CHECKSTACKOVERFLOW();
        memory[sp] = *(byteptr + 1);
        --sp;
        CHECKSTACKOVERFLOW();
        pc = XREG;
        --evalptr;
        return;                 /* Do not advance program counter */
    case VM_RTS:               /* Pop call stack, jump to the address popped.                  */
        CHECKSTACKUNDERFLOW(2);
        ++sp;
        pc = 256 * memory[sp] + memory[sp + 1];
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
	while(memory[XREG]) {
	    printchar(memory[XREG++]);
	}
        --evalptr;
	break;
    case VM_PRMSG:             /* Print literal string at PC (null terminated)                 */
	++pc;
	while(memory[pc]) {
	    printchar(memory[pc++]);
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
    case VM_KBDLN:            /* Obtain line from keyboard and write to memory pointed to by  */
                              /* Y. X contains the max number of bytes in buf. Drop X, Y.     */
        CHECKUNDERFLOW(2);
        getln((char *) &memory[YREG], XREG);
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
};

/*
 * Run the program!
 */
void execute()
{
    while (1) {
        execute_instruction();
    }
}

/*
 * Load bytecode into memory[].
 * TODO: This is POSIX-only at the moment.  Need to add CBM support.
 */
void load()
{
    FILE *fp;
    char ch;
    pc = RTPCSTART;
    fp = fopen("bytecode", "r");
    while (!feof(fp)) {
        ch = fgetc(fp);
        //printhexbyte(ch);
        //printchar('\n');
        memory[pc++] = ch;
    }
    fclose(fp);
    pc = RTPCSTART;
}

int main()
{
    print("EightBallVM v" VERSIONSTR "\n");
    print("Bobbi, 2018\n\n");
    print("Loading bytecode: ");
    load();
#ifdef A2E
    clrscr();
#elif defined(CBM)
    printchar(147); /* Clear */
#endif
    execute();
    return 0;
}
