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
UINT16 tempword;
unsigned short *wordptr;
unsigned char *byteptr;
UINT16 evalstack[EVALSTACKSZ];  /* Evaluation stack - 16 bit ints.  Addressed by evalptr */

#else

/*
 * Zero page variables used on Apple II only at present
 */

extern unsigned char evalptr;
extern UINT16 pc;
extern UINT16 sp;
extern UINT16 fp;
extern UINT16 tempword;
extern unsigned short *wordptr;
extern unsigned char *byteptr;
extern UINT16 evalstack[EVALSTACKSZ];

#pragma zpsym ("evalptr");
#pragma zpsym ("pc");
#pragma zpsym ("sp");
#pragma zpsym ("fp");
#pragma zpsym ("tempword");
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
 * Terminate VM_END
 */
void vm_end() {
    if (evalptr > 0) {
        print("WARNING: evalptr ");
        printdec(evalptr);
        printchar('\n');
    }
#ifdef __GNUC__
    exit(0);
#else
    for (tempword = 0; tempword < 25000; ++tempword);
    exit(0);
#endif
}

/*
 * Pushes the following 16 bit word to the evaluation stack
 */
void vm_ldimm() {
    ++evalptr;
    CHECKOVERFLOW();
    wordptr = (unsigned short *)&MEM(++pc);
    XREG = *wordptr;
    pc += 2;
}

/*
 * Replaces X with 16 bit value pointed to by X
 */
void vm_ldaword() {
    CHECKUNDERFLOW(1);
    wordptr = (unsigned short *)&MEM(XREG);
    XREG = *wordptr;
    ++pc;
}

/*
 * Imm mode - push 16 bit value pointed to by addr after opcode
 */
void vm_ldawordimm() {
    ++evalptr;
    CHECKOVERFLOW();
    wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
    wordptr = (unsigned short *)&MEM(*wordptr); /* Pointer to variable */
    XREG = *wordptr;
    pc += 2;
}

/*
 * Replaces X with 8 bit value pointed to by X
 */
void vm_ldabyte() {
    CHECKUNDERFLOW(1);
    XREG = MEM(XREG);
    ++pc;
}

/*
 * Imm mode - push byte pointed to by addr after opcode
 */
void vm_ldabyteimm() {
    ++evalptr;
    CHECKOVERFLOW();
    wordptr = (unsigned short *)&MEM(++pc);    /* Pointer to operand */
    byteptr = (unsigned char *)&MEM(*wordptr); /* Pointer to variable */
    XREG = *byteptr;
    pc += 2;
}

/*
 * Stores 16 bit value Y in addr pointed to by X. Drops X and Y
 */
void vm_staword() {
    CHECKUNDERFLOW(2);
    wordptr = (unsigned short *)&MEM(XREG);
    *wordptr = YREG;
    evalptr -= 2;
    ++pc;
}

/*
 * Imm mode - store 16 bit value X in addr after opcode. Drop X
 */
void vm_stawordimm() {
    CHECKUNDERFLOW(1);
    wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
    wordptr = (unsigned short *)&MEM(*wordptr); /* Pointer to variable */
    *wordptr = XREG;
    --evalptr;
    pc += 2;
}

/*
 * Stores 8 bit value Y in addr pointed to by X. Drops X and Y
 */
void vm_stabyte() {
    CHECKUNDERFLOW(2);
    MEM(XREG) = YREG;
    evalptr -= 2;
    ++pc;
}

/*
 * Imm mode - store 8 bit value X in addr after opcode. Drop X
 */
void vm_stabyteimm() {
    CHECKUNDERFLOW(1);
    wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
    byteptr = (unsigned char *)&MEM(*wordptr);  /* Pointer to variable */
    *byteptr = XREG;
    --evalptr;
    pc += 2;
}


/*
 * Replaces X with 16 bit value pointed to by X
 */
void vm_ldrword() {
    CHECKUNDERFLOW(1);
#ifdef __GNUC__
    wordptr = (unsigned short *)&MEM((XREG + fp + 1) & 0xffff);
#else
    tempword = XREG + fp + 1;
    wordptr = (unsigned short *)&MEM(tempword);
#endif
    XREG = *wordptr;
    ++pc;
}

/*
 * Imm mode - push 16 bit value pointed to by addr after opcode
 */
void vm_ldrwordimm() {
    ++evalptr;
    CHECKOVERFLOW();
    wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
#ifdef __GNUC__
    wordptr = (unsigned short *)&MEM((*wordptr + fp + 1) & 0xffff); /* Pointer to variable */
#else
    tempword = *wordptr + fp + 1;
    wordptr = (unsigned short *)&MEM(tempword); /* Pointer to variable */
#endif
    XREG = *wordptr;
    pc += 2;
}

/*
 * Replaces X with 8 bit value pointed to by X.
 */
void vm_ldrbyte() {
    CHECKUNDERFLOW(1);
#ifdef __GNUC__
    XREG = MEM((XREG + fp + 1) & 0xffff);
#else
    XREG = MEM(XREG + fp + 1);
#endif
    ++pc;
}

/*
 * Imm mode - push byte pointed to by addr after opcode
 */
void vm_ldrbyteimm() {
    ++evalptr;
    CHECKOVERFLOW();
    wordptr = (unsigned short *)&MEM(++pc);    /* Pointer to operand */
#ifdef __GNUC__
    byteptr = (unsigned char *)&MEM((*wordptr + fp + 1) & 0xffff); /* Pointer to variable */
#else
    tempword = *wordptr + fp + 1;
    byteptr = (unsigned char *)&MEM(tempword); /* Pointer to variable */
#endif
    XREG = *byteptr;
    pc += 2;
}

/*
 * Stores 16 bit value Y in addr pointed to by X. Drops X and Y
 */
void vm_strword() {
    CHECKUNDERFLOW(2);
#ifdef __GNUC__
    wordptr = (unsigned short *)&MEM((XREG + fp + 1) & 0xffff);
#else
    tempword = XREG + fp + 1;
    wordptr = (unsigned short *)&MEM(tempword);
#endif
    *wordptr = YREG;
    evalptr -= 2;
    ++pc;
}

/*
 * Imm mode - store 16 bit value X in addr after opcode. Drop X
 */
void vm_strwordimm() {
    CHECKUNDERFLOW(1);
    wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
#ifdef __GNUC__
    wordptr = (unsigned short *)&MEM((*wordptr + fp + 1) & 0xffff); /* Pointer to variable */
#else
    tempword = *wordptr + fp + 1;
    wordptr = (unsigned short *)&MEM(tempword); /* Pointer to variable */
#endif
    *wordptr = XREG;
    --evalptr;
    pc += 2;
}

/*
 * Stores 8 bit value Y in addr pointed to by X. Drops X and Y
 */
void vm_strbyte() {
    CHECKUNDERFLOW(2);
#ifdef __GNUC__
    MEM((XREG + fp + 1) & 0xffff) = YREG;
#else
    tempword = XREG + fp + 1;
    MEM(tempword) = YREG;
#endif
    evalptr -= 2;
    ++pc;
}

/*
 * Imm mode - store 8 bit value X in addr after opcode. Drop X
 */
void vm_strbyteimm() {
    CHECKUNDERFLOW(1);
    wordptr = (unsigned short *)&MEM(++pc);     /* Pointer to operand */
#ifdef __GNUC__
    byteptr = (unsigned char *)&MEM((*wordptr + fp + 1) & 0xffff);  /* Pointer to variable */
#else
    tempword = *wordptr + fp + 1;
    byteptr = (unsigned char *)&MEM(tempword);  /* Pointer to variable */
#endif
    *byteptr = XREG;
    --evalptr;
    pc += 2;
}

/*
 * Swaps X and Y
 */
void vm_swap() {
    CHECKUNDERFLOW(2);
    tempword = XREG;
    XREG = YREG;
    YREG = tempword;
    ++pc;
}

/*
 * Duplicates X -> X, Y
 */
void vm_dup() {
    CHECKUNDERFLOW(1);
    ++evalptr;
    CHECKOVERFLOW();
    XREG = YREG;
    ++pc;
}

/*
 * Duplicates X -> X,Z; Y -> Y,T
 */
void vm_dup2() {
    CHECKUNDERFLOW(2);
    evalptr += 2;
    CHECKOVERFLOW();
    XREG = ZREG;
    YREG = TREG;
    ++pc;
}

/*
 * Drops X
 */
void vm_drop() {
    CHECKUNDERFLOW(1);
    --evalptr;
    ++pc;
}

/*
 * Duplicates Y -> X,Z
 */
void vm_over() {
    CHECKUNDERFLOW(2);
    ++evalptr;
    CHECKOVERFLOW();
    XREG = ZREG;
    ++pc;
}

/*
 * Duplicates stack level specified in X+1  -> X
 */
void vm_pick() {
        CHECKUNDERFLOW(XREG + 1);
        XREG = evalstack[evalptr - (XREG + 1)];
    ++pc;
}

/*
 * Pop 16 bit value from call stack, push onto eval stack [X]
 */
void vm_popword() {
    CHECKSTACKUNDERFLOW(2);
    sp += 2;
    ++evalptr;
    CHECKOVERFLOW();
#ifdef __GNUC__
    wordptr = (unsigned short *)&MEM(sp - 1);
#else
    tempword = sp - 1;
    wordptr = (unsigned short *)&MEM(tempword);
#endif
    XREG = *wordptr;
    ++pc;
}

/*
 * Pop 8 bit value from call stack, push onto eval stack [X]
 */
void vm_popbyte() {
    CHECKSTACKUNDERFLOW(1);
    ++sp;
    ++evalptr;
    CHECKOVERFLOW();
    XREG = MEM(sp);
    ++pc;
}

/*
 * Push 16 bit value in X onto call stack.  Drop X
 */
void vm_pshword() {
    CHECKUNDERFLOW(1);
    byteptr = (unsigned char *)&XREG;
    MEM(sp--) = *(byteptr + 1);
    CHECKSTACKOVERFLOW();
    MEM(sp--) = *byteptr;
    CHECKSTACKOVERFLOW();
    --evalptr;
    ++pc;
}

/*
 * Push 8 bit value in X onto call stack.  Drop X
 */
void vm_pshbyte() {
    CHECKUNDERFLOW(1);
    MEM(sp--) = XREG & 0x00ff;
    CHECKSTACKOVERFLOW();
    --evalptr;
    ++pc;
}

/*
 * Discard X bytes from call stack.  Drop X
 */
void vm_discard() {
    CHECKUNDERFLOW(1);
    sp += XREG;
    --evalptr;
    ++pc;
}

/*
 * Copy stack pointer to frame pointer. (Enter function scope)
 */
void vm_sptofp() {
    /* Push old FP to stack */
    byteptr = (unsigned char *)&fp;
    MEM(sp--) = *(byteptr + 1);
    CHECKSTACKOVERFLOW();
    MEM(sp--) = *byteptr;
    CHECKSTACKOVERFLOW();
    fp = sp;
    ++pc;
}

/*
 * Copy frame pointer to stack pointer. (Release local vars)
 */
void vm_fptosp() {
    sp = fp;
    /* Pop old FP from stack -> FP */
    CHECKSTACKUNDERFLOW(2);
    sp += 2;
    CHECKOVERFLOW();
#ifdef __GNUC__
    wordptr = (unsigned short *)&MEM(sp - 1);
#else
    tempword = sp - 1;
    wordptr = (unsigned short *)&MEM(tempword);
#endif
    fp = *wordptr;
    ++pc;
}

/*
 * Convert absolute address in X to relative address
 */
void vm_ator() {
#ifdef __GNUC__
    XREG = (XREG - fp - 1) & 0xffff;
#else
    XREG = XREG - fp - 1;
#endif
    ++pc;
}

/*
 * Convert relative address in X to absolute address
 */
void vm_rtoa() {
#ifdef __GNUC__
    XREG = (XREG + fp + 1) & 0xffff;
#else
    XREG = XREG + fp + 1;
#endif
    ++pc;
}

/*
 * X = X+1
 */
void vm_inc() {
    CHECKUNDERFLOW(1);
    ++XREG;
    ++pc;
}

/*
 * X = X-1
 */
void vm_dec() {
    CHECKUNDERFLOW(1);
    --XREG;
    ++pc;
}

/*
 * X = Y+X.  Y is dropped
 */
void vm_add() {
    CHECKUNDERFLOW(2);
    YREG = YREG + XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y-X.  Y is dropped
 */
void vm_sub() {
    CHECKUNDERFLOW(2);
    YREG = YREG - XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y*X.  Y is dropped
 */
void vm_mul() {
    CHECKUNDERFLOW(2);
    YREG = YREG * XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y/X. Y is dropped
 */
void vm_div() {
    CHECKUNDERFLOW(2);
    YREG = YREG / XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y%X. Y is dropped
 */
void vm_mod() {
    CHECKUNDERFLOW(2);
    YREG = YREG % XREG;
    --evalptr;
    ++pc;
}

/*
 * X = -X
 */
void vm_neg() {
    CHECKUNDERFLOW(1);
    XREG = -XREG;
    ++pc;
}

/*
 * X = Y>X.  Y is dropped
 */
void vm_gt() {
    CHECKUNDERFLOW(2);
    YREG = YREG > XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y>=X. Y is dropped
 */
void vm_gte() {
    CHECKUNDERFLOW(2);
    YREG = YREG >= XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y<X.  Y is dropped
 */
void vm_lt() {
    CHECKUNDERFLOW(2);
    YREG = YREG < XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y<=X. Y is dropped
 */
void vm_lte() {
    CHECKUNDERFLOW(2);
    YREG = YREG <= XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y==X. Y is dropped
 */
void vm_eql() {
    CHECKUNDERFLOW(2);
    YREG = YREG == XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y!=X. Y is dropped
 */
void vm_neql() {
    CHECKUNDERFLOW(2);
    YREG = YREG != XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y&&X. Y is dropped
 */
void vm_and() {
    CHECKUNDERFLOW(2);
    YREG = YREG && XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y||X. Y is dropped
 */
void vm_or() {
    CHECKUNDERFLOW(2);
    YREG = YREG || XREG;
    --evalptr;
    ++pc;
}

/*
 * X = !X
 */
void vm_not() {
    CHECKUNDERFLOW(1);
    XREG = !XREG;
    ++pc;
}

/*
 * X = Y&X. Y is dropped
 */
void vm_bitand() {
    CHECKUNDERFLOW(2);
    YREG = YREG & XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y|X. Y is dropped
 */
void vm_bitor() {
    CHECKUNDERFLOW(2);
    YREG = YREG | XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y^X. Y is dropped
 */
void vm_bitxor() {
    CHECKUNDERFLOW(2);
    YREG = YREG ^ XREG;
    --evalptr;
    ++pc;
}

/*
 * X = ~X
 */
void vm_bitnot() {
    CHECKUNDERFLOW(1);
    XREG = ~XREG;
    ++pc;
}

/*
 * X = Y<<X. Y is dropped
 */
void vm_lsh() {
    CHECKUNDERFLOW(2);
    YREG = YREG << XREG;
    --evalptr;
    ++pc;
}

/*
 * X = Y>>X. Y is dropped
 */
void vm_rsh() {
    CHECKUNDERFLOW(2);
    YREG = YREG >> XREG;
    --evalptr;
    ++pc;
}

/*
 * Jump to address X.  Drop X
 */
void vm_jmp() {
    CHECKUNDERFLOW(1);
    pc = XREG;
    --evalptr;
}

/*
 * Imm mode - jump to 16 bit word following opcode
 */
void vm_jmpimm() {
    wordptr = (unsigned short *)&MEM(++pc);
    pc = *wordptr;
}

/*
 * If Y!= 0, jump to address X.  Drop X, Y
 */
void vm_brnch() {
    CHECKUNDERFLOW(2);
    if (YREG) {
        pc = XREG;
    } else {
        ++pc;
    }
    evalptr -= 2;
}

/*
 * Imm mode - if X!=0 branch to 16 bit word following opcode
 */
void vm_brnchimm() {
    wordptr = (unsigned short *)&MEM(++pc);
    CHECKUNDERFLOW(1);
    if (XREG) {
        pc = *wordptr;
    } else {
        pc += 2;
    }
    --evalptr;
}

/*
 * Push PC to call stack.  Jump to address X.  Drop X
 */
void vm_jsr() {
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
}

/*
 * Imm mode - push PC to call stack, jump to 16 bit word
 */
void vm_jsrimm() {
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
}

/*
 * Pop call stack, jump to the address popped
 */
void vm_rts() {
    CHECKSTACKUNDERFLOW(2);
    ++sp;
    wordptr = (unsigned short *)&MEM(sp);
    pc = *wordptr;
    ++sp;
    ++pc;
}

/*
 * Print 16 bit decimal in X.  Drop X
 */
void vm_prdec() {
    CHECKUNDERFLOW(1);
    printdec(XREG);
    --evalptr;
    ++pc;
}

/*
 * Print 16 bit hex in X.  Drop X
 */
void vm_prhex() {
    CHECKUNDERFLOW(1);
    printhex(XREG);
    --evalptr;
    ++pc;
}

/*
 * Print character in X.  Drop X
 */
void vm_prch() {
    CHECKUNDERFLOW(1);
    printchar((unsigned char) XREG);
    --evalptr;
    ++pc;
}

/*
 * Print null terminated string pointed to by X.  Drop X
 */
void vm_prstr() {
    CHECKUNDERFLOW(1);
    while (MEM(XREG)) {
        printchar(MEM(XREG++));
    }
    --evalptr;
    ++pc;
}

/*
 * Print literal string at PC (null terminated)
 */
void vm_prmsg() {
    ++pc;
    while (MEM(pc)) {
        printchar(MEM(pc++));
    }
    ++pc;
}

/*
 * Push character from keyboard onto eval stack
 */
void vm_kbdch() {
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
    ++pc;
}

/*
 * Obtain line from keyboard and write to memory pointed to by
 * Y. X contains the max number of bytes in buf. Drop X, Y
 */
void vm_kbdln() {
    CHECKUNDERFLOW(2);
    getln((char *) &MEM(YREG), XREG);
    evalptr -= 2;
    ++pc;
}

typedef void (*func)(void);

/*
 * Must be in same order as enum bytecode.
 */
func jumptbl[] = {
    vm_end,
    vm_ldimm,
    vm_ldaword,
    vm_ldawordimm,
    vm_ldabyte,
    vm_ldabyteimm,
    vm_staword,
    vm_stawordimm,
    vm_stabyte,
    vm_stabyteimm,
    vm_ldrword,
    vm_ldrwordimm,
    vm_ldrbyte,
    vm_ldrbyteimm,
    vm_strword,
    vm_strwordimm,
    vm_strbyte,
    vm_strbyteimm,
    vm_swap,
    vm_dup,
    vm_dup2,
    vm_drop,
    vm_over,
    vm_pick,
    vm_popword,
    vm_popbyte,
    vm_pshword,
    vm_pshbyte,
    vm_discard,
    vm_sptofp,
    vm_fptosp,
    vm_ator,
    vm_rtoa,
    vm_inc,
    vm_dec,
    vm_add,
    vm_sub,
    vm_mul,
    vm_div,
    vm_mod,
    vm_neg,
    vm_gt,
    vm_gte,
    vm_lt,
    vm_lte,
    vm_eql,
    vm_neql,
    vm_and,
    vm_or,
    vm_not,
    vm_bitand,
    vm_bitor,
    vm_bitxor,
    vm_bitnot,
    vm_lsh,
    vm_rsh,
    vm_jmp,
    vm_jmpimm,
    vm_brnch,
    vm_brnchimm,
    vm_jsr,
    vm_jsrimm,
    vm_rts,
    vm_prdec,
    vm_prhex,
    vm_prch,
    vm_prstr,
    vm_prmsg,
    vm_kbdch,
    vm_kbdln,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported,
    unsupported /* Should be at least 255 lines long */
};

/*
 * Fetch, decode and execute a VM instruction.
 * Advance program counter and loop until VM_END.
 */
void execute()
{
#ifdef DEBUGREGS
    unsigned short i;
#endif

    evalptr = 0;
    pc = RTPCSTART;
    sp = fp = RTCALLSTACKTOP;

    while (1) {

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

#ifndef A2E
    jumptbl[MEM(pc)]();
#else
#if 0
    /* Slight speedup versus the code emitted by cc65 */
    __asm__("ml: lda     (_pc)");
    __asm__("    stz     tmp1");
    __asm__("    asl     a");
    __asm__("    rol     tmp1");
    __asm__("    clc");
    __asm__("    adc     #<(_jumptbl)");
    __asm__("    sta     ptr1");
    __asm__("    lda     tmp1");
    __asm__("    adc     #>(_jumptbl)");
    __asm__("    sta     ptr1+1");
    __asm__("    ldy     #$01");
    __asm__("    lda     (ptr1),y");
    __asm__("    tax");
    __asm__("    lda     (ptr1)");
    __asm__("    jsr     callax");
    __asm__("    bra     ml");
#else
    /* This version assumes (_pc) < 128, saves a few more cycles */
    /* Slight speedup versus the code emitted by cc65 */
    __asm__("ml: lda     (_pc)");
    __asm__("    asl     a");
    __asm__("    clc");
    __asm__("    adc     #<(_jumptbl)");
    __asm__("    sta     ptr1");
    __asm__("    lda     #$00");
    __asm__("    adc     #>(_jumptbl)");
    __asm__("    sta     ptr1+1");
    __asm__("    ldy     #$01");
    __asm__("    lda     (ptr1),y");
    __asm__("    tax");
    __asm__("    lda     (ptr1)");
    __asm__("    jsr     callax");
    __asm__("    bra     ml");
#endif
#endif

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
