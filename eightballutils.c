/**************************************************************************/
/* EightBall                                                              */
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
/* Copyright Bobbi Webber-Manners 2016, 2017, 2018                        */
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

#include "eightballutils.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef A2E
#include <apple2enh.h>
#include <peekpoke.h>
#include <conio.h>
#endif

/*
 * This does the same thing as fputs(str, 1), but uses marginally less
 * memory.
 */
void print(char *str) {
	write(1, str, strlen(str));
}

/*
 * This does the same thing as printchar() but uses marginally less memory.
 */
void printchar(char c) {
	write(1, &c, 1);
}

/*
 * Print a 16 bit integer value as an unsigned decimal
 */
void printdec(unsigned int val) {

	unsigned char digit;
	unsigned int denom = 10000;
	unsigned char doprint = 0;

	do {
		digit = val / denom;
		if (digit) {
			doprint = 1;
		}
		if (doprint) {
			printchar(digit + '0');
		}
		val = val % denom;
		denom = denom / 10;
	} while (denom);
	if (!doprint) {
		printchar('0');
	}
}

/*
 * Return character for hex digit 0 to 15
 */
char hexval2char(unsigned char val) {
	if (val > 9) {
		return val - 10 + 'a';
	}
	return val + '0';
}

/*
 * Print a value as hex
 */
void printhex(unsigned int val) {

	printchar('$');
	printchar(hexval2char((val>>12) & 0x0f));
	printchar(hexval2char((val>>8) & 0x0f));
	printchar(hexval2char((val>>4) & 0x0f));
	printchar(hexval2char(val & 0x0f));
}

/*
 * Print a value as hex
 */
void printhexbyte(unsigned char val) {

	printchar('$');
	printchar(hexval2char((val>>4) & 0x0f));
	printchar(hexval2char(val & 0x0f));
}

#ifdef A2E
#define KEY_BACKSPACE 127
#define KEY_LEFTARROW 8
#endif

/*
 * This is lighter than gets() and also safe!
 * Will read up to buflen bytes from STDIN
 * Has some ugly special case code for Apple II.
 */
void getln(char *str, unsigned char buflen)
{
    unsigned char i;
#ifdef A2E
    unsigned char key;
    unsigned char xpos;
    unsigned char ypos;
#endif
    unsigned char j = 0;
    do {
        i = read(0, str + j, 1);
#ifdef A2E
        /*
         * Handle backspace and delete keys
         * TODO: I would sooner not use these conio functions.
         * However this works for now.
         * TODO: This assumes 80 column mode and does strange things
         * in 40 cols!
         */
        key = *(str + j);
        if (key == KEY_BACKSPACE) {
            xpos = wherex();
            ypos = wherey();
            if ((xpos == 1) && (ypos != 0)) {
                xpos = 79;
                --ypos;
            } else if ((xpos == 0) && (ypos != 0)) {
                xpos = 78;
                --ypos;
            } else if (xpos > 1) {
                xpos -= 2;
            }
            gotoxy(xpos, ypos);
        }
        if ((key == KEY_LEFTARROW) || (key == KEY_BACKSPACE)) {
            --j;
        } else {
            ++j;
        }
#else
        ++j;
#endif
    } while ((i) && (j < buflen) && *(str + j - 1) != '\n');
    str[j - 1] = '\0';
}

#ifdef A2E
/*
 * This is for Apple II only.  Obtain keypress.
 */
char getkey(void)
{
    char junk;
    char ch = PEEK(0xc000);
    if (ch > 128) {
        junk = PEEK(0xc010);    /* Clear kbd strobe */
        return ch - 128;
    }
    return 0;
}
#endif

/*
 * For Apple II and CBM.
 * Returns 1 if interrupted by user, 0 otherwise
 */
unsigned char checkInterrupted(void)
{
#ifdef A2E
    char junk;
#endif
#ifdef CBM
    /* Check for STOP key */
    if (PEEK(0x00c5) == 24) {
        return 1;
    }
#elif defined(A2E)
    /* Check for ESC key */
    if (PEEK(0xc000) == 0x9b) {
        junk = PEEK(0xc010);    /* Clear kbd strobe */
        return 1;
    }
#endif
    return 0;
}

