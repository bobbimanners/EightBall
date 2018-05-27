/**************************************************************************/
/* EightBall Disassembler                                                 */
/*                                                                        */
/* The Eight Bit Algorithmic Language                                     */
/* For Apple IIe/c/gs (64K), Commodore 64, VIC-20 +32K RAM expansion      */
/* (also builds for Linux as 32 bit executable (gcc -m32) only)           */
/*                                                                        */
/* Compiles with cc65 v2.15 for VIC-20, C64, Apple II                     */
/* and gcc 7.3 for Linux                                                  */
/*                                                                        */
/* cc65: Define symbol VIC20 to build for Commodore VIC-20 + 32K.         */
/*       Define symbol C64 to build for Commodore 64.                     */
/*       Define symbol A2E to build for Apple //e.                        */
/*                                                                        */
/* Copyright Bobbi Webber-Manners 2018                                    */
/* EightBall VM disassembler                                              */
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

#include "eightballvm.h"
#include "eightballutils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef A2E
#include <conio.h>
#endif

/* Order must match enum bytecode */
char *bytecodenames[] = {
    "END",
    "LDI",
    "LDAW",
    "LDAWI",
    "LDAB",
    "LDABI",
    "STAW",
    "STAWI",
    "STAB",
    "STABI",
    "LDRW",
    "LDRWI",
    "LDRB",
    "LDRBI",
    "STRW",
    "STRWI",
    "STRB",
    "STRBI",
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
    "DISC",
    "SPFP",
    "FPSP",
    "ATOR",
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
    "JMPI",
    "BRC",
    "BRCI",
    "JSR",
    "JSRI",
    "RTS",
    "PRDEC",
    "PRHEX",
    "PRCH",
    "PRSTR",
    "PRMSG",
    "KBDCH",
    "KBDLN"
};

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

#ifdef __GNUC__
unsigned char memory[MEMORYSZ];
#else
unsigned char *memory = 0;
#endif

#ifdef __GNUC__
#define UINT16 unsigned short
#else
#define UINT16 unsigned int
#endif

UINT16 pc = RTPCSTART;          /* Program counter */
UINT16 lastpc;

/*
 * Print a value as hex
 */
void _printhexbyte(unsigned char val) {

    printchar(hexval2char((val>>4) & 0x0f));
    printchar(hexval2char(val & 0x0f));
}

/*
 * Fetch, decode and disassemble a VM instruction, then advance the program counter.
 */
void disassemble_instruction()
{
    printhex(pc);
    print(": ");
    _printhexbyte(memory[pc]);
    printchar(' ');
    switch(memory[pc++]) {
      case VM_LDIMM:
      case VM_LDAWORDIMM:
      case VM_LDABYTEIMM:
      case VM_LDRWORDIMM:
      case VM_LDRBYTEIMM:
      case VM_STAWORDIMM:
      case VM_STABYTEIMM:
      case VM_STRWORDIMM:
      case VM_STRBYTEIMM:
      case VM_JMPIMM:
      case VM_BRNCHIMM:
      case VM_JSRIMM:
        _printhexbyte(memory[pc++]);
        printchar(' ');
        _printhexbyte(memory[pc++]);
        print("   ");
        print(bytecodenames[memory[pc-3]]);
        printchar(' ');
        printhex(memory[pc-2] + (memory[pc-1] << 8));
        print(" (");
        printdec(memory[pc-2] + (memory[pc-1] << 8));
        print(")");
        break;
      case VM_PRMSG:
        print("...00   ");
        print(bytecodenames[memory[pc-1]]);
        print(" \"");
        while(memory[pc]) {
           printchar(memory[pc++]);
        }
        ++pc;
        printchar('"');
        break;
      default:
        print("        ");
        if (memory[pc-1] <= VM_KBDLN) {
            print(bytecodenames[memory[pc-1]]);
        } else {
            print("**ILLEGAL**");
        }
    }
#ifdef A2E
    printchar('\r');
#else
    printchar('\n');
#endif
};

/*
 * Disassemble the program!
 */
void disassemble()
{
    while (pc < lastpc) {
        disassemble_instruction();
    }
}

/*
 * Load bytecode into memory[].
 */
void load()
{
    FILE *fp;
    char ch;
    char *p = (char*)&memory[RTPCSTART];

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
        memory[pc++] = ch;
        /* Print dot for each page */
        if (pc%0xff == 0) {
            printchar('.');
        }
    }
    fclose(fp);
    lastpc = pc - 1;
    pc = RTPCSTART;
#ifdef A2E
    printchar(7);
#endif
}

int main()
{
    print("EightBall Disassembler v" VERSIONSTR "\n");
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
    disassemble();
#ifdef A2E
    cgetc();
#endif
    return 0;
}
