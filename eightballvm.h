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
 * EIGHTBALL VIRTUAL MACHINE DEFINITION - Compilation Target
 *
 * Simple stack based VM with EVALUATION STACK of 16 bit integer values.
 * It doesn't need to be too big - 16 words is a reasonable size for this
 * stack.
 *
 * For convenience define this shorthand:
 * [ X ] Top of stack
 * [ Y ] Second
 * [ Z ] Third
 * [ T ] Fourth
 *
 * There is a separate CALL STACK which is used for argument passing,
 * subroutine call and return and for local variables.  This stack can grow
 * very large, especially if you allocate big local arrays.  The call stack
 * allows either 16 bit words or bytes to be pushed or popped.
 *
 * The virtual machine also has the following three 16 bit registers:
 *   Program Counter (Referred to as rtPC in the compiler code)
 *   Stack Pointer (Referred to as rtSP in the compiler code)
 *                  Points to free memory one 'above' the top of the call stack.
 *   Frame Pointer (points to stack pointer on entry to subroutine.  Used to make
 *                  it easier to refer to local variables allocated on the call
 *                  stack, and also to release the locals on return from sub.)
 */
enum bytecode {
	/**** Miscellaneous ********************************************************/
	VM_END,    /* Terminate execution                                          */
	/**** Load and Store *******************************************************/
	VM_LDIMM,  /* Pushes the following 16 bit word to the evaluation stack     */
	/* Absolute addressing:                                                    */
       	VM_LDAWORD,/* Replaces X with 16 bit value pointed to by X.                */
       	VM_LDABYTE,/* Replaces X with 8 bit value pointed to by X.                 */
       	VM_STAWORD,/* Stores 16 bit value Y in addr pointed to by X. Drops X and Y.*/
       	VM_STABYTE,/* Stores 8 bit value Y in addr pointed to by X. Drops X and Y. */
	/* Relative to Frame Pointer addressing:                                   */
       	VM_LDRWORD,/* Replaces X with 16 bit value pointed to by X.                */
       	VM_LDRBYTE,/* Replaces X with 8 bit value pointed to by X.                 */
       	VM_STRWORD,/* Stores 16 bit value Y in addr pointed to by X. Drops X and Y.*/
       	VM_STRBYTE,/* Stores 8 bit value Y in addr pointed to by X. Drops X and Y. */
	/**** Manipulate evaluation stack ******************************************/
	VM_SWAP,   /* Swaps X and Y                                                */
	VM_DUP,    /* Duplicates X -> X, Y                                         */
	VM_DUP2,   /* Duplicates X -> X,Z; Y -> Y,T                                */
	VM_DROP,   /* Drops X                                                      */
	VM_OVER,   /* Duplicates Y -> X,Z                                          */
	VM_PICK,   /* Duplicates stack level specified in X+1 -> X                 */
	/**** Manipulate call stack ************************************************/
	VM_POPWORD,/* Pop 16 bit value from call stack, push onto eval stack [X]   */
	VM_POPBYTE,/* Pop 8 bit value from call stack, push onto eval stack [X]    */
	VM_PSHWORD,/* Push 16 bit value in X onto call stack.  Drop X.             */
	VM_PSHBYTE,/* Push 8 bit value in X onto call stack.  Drop X.              */
	VM_SPTOFP, /* Copy stack pointer to frame pointer. (Enter function scope)  */
	VM_FPTOSP, /* Copy frame pointer to stack pointer. (Release local vars)    */
	VM_RTOA,   /* Convert relative address in X to absolute address            */
	/**** Integer math *********************************************************/
	VM_INC,    /* X = X+1.                                                     */
	VM_DEC,    /* X = X-1.                                                     */
	VM_ADD,    /* X = Y+X.  Y is dropped.                                      */
	VM_SUB,    /* X = Y-X.  Y is dropped.                                      */
	VM_MUL,    /* X = Y*X.  Y is dropped.                                      */
	VM_DIV,    /* X = Y/X.  Y is dropped.                                      */
	VM_MOD,    /* X = Y%X.  Y is dropped                                     . */
	VM_NEG,    /* X = -X                                                       */
	/**** Comparisons **********************************************************/
	VM_GT,     /* X = Y>X.  Y is dropped.                                      */
	VM_GTE,    /* X = Y>=X. Y is dropped.                                      */
	VM_LT,     /* X = Y<X.  Y is dropped.                                      */
	VM_LTE,    /* X = Y<=X. Y is dropped.                                      */
	VM_EQL,    /* X = Y==X. Y is dropped.                                      */
	VM_NEQL,   /* X = Y!=X. Y is dropped.                                      */
	/**** Logical operations ***************************************************/
	VM_AND,    /* X = Y&&X. Y is dropped.                                      */
	VM_OR,     /* X = Y||X. Y is dropped.                                      */
	VM_NOT,    /* X = !X                                                       */
	/**** Bitwise operations ***************************************************/
	VM_BITAND, /* X = Y&X. Y is dropped.                                       */
	VM_BITOR,  /* X = Y|X. Y is dropped.                                       */
	VM_BITXOR, /* X = Y^X. Y is dropped.                                       */
	VM_BITNOT, /* X = ~X.                                                      */
	VM_LSH,    /* X = Y<<X. Y is dropped.                                      */
	VM_RSH,    /* X = Y>>X. Y is dropped.                                      */
	/**** Flow control *********************************************************/
	VM_JMP,    /* Jump to address X.  Drop X.                                  */
	VM_BRNCH,  /* If Y!= 0, jump to address X.  Drop X, Y.                     */
	VM_JSR,    /* Push PC to call stack.  Jump to address X.  Drop X.          */
	VM_RTS,    /* Pop call stack, jump to the address popped.                  */
	/**** Input / Output *******************************************************/
	VM_PRDEC,  /* Print 16 bit decimal in X.  Drop X                           */
	VM_PRHEX,  /* Print 16 bit hex in X.  Drop X                               */
	VM_PRCH,   /* Print character in X.  Drop X                                */
	VM_PRSTR,  /* Print null terminated string pointed to by X.  Drop X        */
	VM_PRMSG,  /* Print literal string at PC (null terminated)                 */
	VM_KBDCH,  /* Push character from keyboard onto eval stack                 */
	VM_KBDLN   /* Obtain line from keyboard and write to memory pointed to by  */
	/*            Y. X contains the max number of bytes in buf. Drop X, Y.     */
	/***************************************************************************/
};

/* Order must match enum bytecode */
char *bytecodenames[] = {
	"END",
	"LDI",
	"LDAW",
	"LDAB",
	"STAW",
	"STAB",
	"LDRW",
	"LDRB",
	"STRW",
	"STRB",
	"SWP",
	"DUP",
	"DUP2",
	"DRP",
	"OVER",
	"PICK",
	"POPW",
	"POPB",
	"PSHW",
	"PSHB",
	"SPFP",
	"FPSP",
	"RTOA",
	"INC",
	"DEC",
	"ADD",
	"SUB",
	"MUL",
	"DIV",
	"MOD",
	"NEG",
	"GT",
	"GTE",
	"LT",
	"LTE",
	"EQL",
	"NEQL",
	"AND",
	"OR",
	"NOT",
	"BAND",
	"BOR",
	"BXOR",
	"BNOT",
	"LSH",
	"RSH",
	"JMP",
	"BRC",
	"JSR",
	"RTS",
	"PRDEC",
	"PRHEX",
	"PRCH",
	"PRSTR",
        "PRMSG",
        "KBDCH",
	"KBDLN"
};

#ifdef A2E

/*
 * Apple II Enhanced
 */
#define RTCALLSTACKTOP 0xb7ff
#define RTCALLSTACKLIM 0x9800
#define RTPCSTART      0x5000 /* TBC */

#elif defined(C64)

/*
 * C64
 */
#define RTCALLSTACKTOP 0xbfff
#define RTCALLSTACKLIM 0xa000
#define RTPCSTART      0x3000 /* TBC */

#elif defined(VIC20)

/*
 * VIC-20:
 */
#define RTCALLSTACKTOP 0xbfff
#define RTCALLSTACKLIM 0xa000
#define RTPCSTART      0x4000 /* TBC */

#elif defined(__GNUC__)

/*
 * Linux
 */
//#define RTCALLSTACKTOP 64 * 1024 - 1
#define RTCALLSTACKTOP 48 * 1024 - 1  // FOR TESTING
#define RTCALLSTACKLIM 32 * 1024
#define RTPCSTART 0
#endif

