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
/* Formatted using indent -kr -nut                                        */
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

//#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <unistd.h>

#include "eightballutils.h"
#include "eightballvm.h"

/* Shortcut define CC65 makes code clearer */
#if defined(VIC20) || defined(C64) || defined(A2E)
#define CC65
#endif

/* Shortcut define CBM is useful! */
#if defined(VIC20) || defined(C64)
#define CBM
#endif

#ifdef CC65
#ifdef CBM
/* Commodore headers */
#include <cbm.h>
#include <peekpoke.h>
#endif
#if defined(A2E)
/* Apple //e headers */
#include <apple2enh.h>
#include <fcntl.h>              /* For open(), close() */
#include <unistd.h>             /* For read(), write() */
#include <conio.h>              /* For clrscr() */
#include <stdio.h>              /* For fopen(), fclose() */
#include <peekpoke.h>
#endif
#endif

#ifdef __GNUC__
#include <stdio.h>              /* For FILE */
#endif

//#define TEST
//#define DEBUG_READFILE

//#define EXIT(arg) {printf("%d\n",__LINE__); exit(arg);}
#define EXIT(arg) exit(arg)


/*
 ***************************************************************************
 * Lightweight functions (lighter than the cc65 library for our use case)
 ***************************************************************************
 */

/*
 * This should use less memory than the table-driven version of isalpha()
 * provided by cc65
 */
#define isalphach(ch) ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))

/*
 * This should use less memory than the table-driven version of isdigit()
 * provided by cc65
 */
#define isdigitch(ch) (ch >= '0' && ch <= '9')

/*
 * Implement this ourselves for cc65 - x^y
 */
int _pow(int x, int y)
{
    int i;
    int ret = 1;

    for (i = 0; i < y; i++) {
        ret *= x;
    }
    return ret;
}

/*
 ***************************************************************************
 * Prototypes
 ***************************************************************************
 */
unsigned char P(void);
unsigned char E(void);
unsigned char eval(unsigned char checkNoMore, int *val);
unsigned char parseint(int *);
unsigned char parsehexint(int *);
unsigned char getintvar(char *, int, int *, unsigned char *,
                        unsigned char);
unsigned char openfile(unsigned char);
unsigned char readfile(void);
unsigned char writefile(void);
void list(unsigned int, unsigned int);
void run(unsigned char);
void new(void);
unsigned char parseline(void);
unsigned char docall(void);
unsigned char doreturn(int retvalue);
void emit(enum bytecode code);
void emitldi(int word, char *comment);
void emitprmsg(void);
void linksubs(void);

/*
 ***************************************************************************
 * Globals
 ***************************************************************************
 */

char compile = 0;               /* 0 means interpret, 1 means compile          */
char compilingsub = 0;          /* 1 when compiling subroutine, 0 otherwise    */
char readbuf[255];              /* Buffer for reading from file                */
char lnbuf[255];                /* Input text line buffer                      */
char *txtPtr;                   /* Pointer to next character to read in lnbuf  */

#define STACKSZ 16              /* Size of expression stacks   */
#define RETSTACKSZ 64           /* Size of return stack        */

int operand_stack[STACKSZ];     /* Operand stack - grows down  */
unsigned char operator_stack[STACKSZ];  /* Operator stack - grows down */
int return_stack[RETSTACKSZ];   /* Return stack - grows down   */

unsigned char operatorSP;       /* Operator stack pointer      */
unsigned char operandSP;        /* Operand stack pointer       */
unsigned char returnSP;         /* Return stack pointer        */

jmp_buf jumpbuf;                /* For setjmp()/longjmp()      */

#ifndef CBM
FILE *fd;                       /* File descriptor             */
#endif

/*
 * Definitions for the EightBall VM - compilation target
 */

unsigned int rtPC;              /* Program counter when compiling      */
unsigned int rtSP;              /* Stack pointer when compiling        */
unsigned int rtFP;              /* Frame pointer when compiling        */
unsigned int rtPCBeforeEval;    /* Stashed copy of program counter     */
unsigned char *codeptr;         /* Pointer to write VM code to memory  */

/*
 * Represents a line of EightBall code.
 */
struct lineofcode {
    char *line;
    struct lineofcode *next;
};

/*
 * Pointer to current line
 */
struct lineofcode *current = NULL;

/*
 * Used as a line number counter
 */
int counter;

/*
 * Holds the return value from a subroutine / function
 */
int retregister = 0;

/*
 ***************************************************************************
 * Token table for expression parser
 ***************************************************************************
 */

/*
 * Single character binary operators - order must match binaryops string.
 * and must be sequential.
 */
#ifdef CBM
const char binaryops[] = "^/%*+-><&#!";
#else
const char binaryops[] = "^/%*+-><&|!";
#endif

#define TOK_POW     245         /* Binary ^               */
#define TOK_DIV     246         /* Binary /               */
#define TOK_MOD     247         /* Binary %               */
#define TOK_MUL     248         /* Binary *               */
#define TOK_ADD     249         /* Binary +               */
#define TOK_SUB     250         /* Binary -               */
#define TOK_GT      251         /* Binary >               */
#define TOK_LT      252         /* Binary <               */
#define TOK_BITAND  253         /* Binary &               */
#define TOK_BITOR   254         /* Binary | (# on CBM)    */
#define TOK_BITXOR  255         /* Binary ! (^ in C code) */

/*
 * Macro to determine if a token is a binary operator with one character.
 */
#define IS1CHBINARY(tok) ((tok >= TOK_POW) && (tok <= TOK_BITXOR))

/*
 * Two character binary operators - order must match binaryops1/2 strings.
 * and must be sequential.
 */
#ifdef CBM
const char binaryops1[] = "=!><&#<>";   /* 1st char */
const char binaryops2[] = "====&#<>";   /* 2nd char */
#else
const char binaryops1[] = "=!><&|<>";   /* 1st char */
const char binaryops2[] = "====&|<>";   /* 2nd char */
#endif

#define TOK_EQL     237         /* Binary ==             */
#define TOK_NEQL    238         /* Binary !=             */
#define TOK_GTE     239         /* Binary >=             */
#define TOK_LTE     240         /* Binary <=             */
#define TOK_AND     241         /* Binary &&             */
#define TOK_OR      242         /* Binary || (## on CBM) */
#define TOK_LSH     243         /* Binary <<             */
#define TOK_RSH     244         /* Binary >>             */

/*
 * Macro to determine if a token is a binary operator with two characters.
 */
#define IS2CHBINARY(tok) ((tok >= TOK_EQL) && (tok <= TOK_RSH))

/*
 * Unary operators - order must match unaryops string and must be sequential.
 * All unary operators are single character.
 */
#ifdef CBM
const char unaryops[] = "-+!.*^";
#else
const char unaryops[] = "-+!~*^";
#endif

#define TOK_UNM     231         /* Unary -                  */
#define TOK_UNP     232         /* Unary +                  */
#define TOK_NOT     233         /* Unary !                  */
#define TOK_BITNOT  234         /* Unary ~ (. on CBM)       */
#define TOK_STAR    235         /* Unary * (word deref.)    */
#define TOK_CARET   236         /* Unary ^ (byte deref.)    */

/*
 * Macro to determine if a token is a unary operator.
 */
#define ISUNARY(tok) ((tok >= TOK_UNM) && (tok <= TOK_CARET))

/*
 * Special token to mark end of stack
 */
#define SENTINEL    50

/*
 * Special token for illegal operator or statement.
 */
#define ILLEGAL     100

#define ERR_SYNTAX  100         /* Syntax             */
#define ERR_NOIF    101         /* No IF              */
#define ERR_NOFOR   102         /* No FOR             */
#define ERR_NOWHILE 103         /* No WHILE           */
#define ERR_NOSUB   104         /* No SUB             */
#define ERR_STACK   105         /* No stack           */
#define ERR_COMPLEX 106         /* Too complex        */
#define ERR_VAR     107         /* Variable expected  */
#define ERR_REDEF   108         /* Variable redefined */
#define ERR_EXPECT  109         /* Expected character */
#define ERR_EXTRA   110         /* Unexpected extra   */
#define ERR_DIM     111         /* Bad dimension      */
#define ERR_SUBSCR  112         /* Bad subscript      */
#define ERR_RUNSUB  113         /* Ran into sub       */
#define ERR_STR     114         /* Bad string         */
#define ERR_FILE    115         /* File error         */
#define ERR_LINE    116         /* Bad line number    */
#define ERR_EXPR    117         /* Invalid expr       */
#define ERR_NUM     118         /* Invalid number     */
#define ERR_ARG     119         /* Argument error     */
#define ERR_TYPE    120         /* Type error         */
#define ERR_DIVZERO 121         /* Divide by zero     */
#define ERR_LINK    122         /* Linkage error      */

/*
 * Error reporting
 */
void error(unsigned char errcode)
{
    printchar('?');
    switch (errcode) {
    case ERR_SYNTAX:
        print("syntax");
        break;
    case ERR_NOIF:
        print("no if");
        break;
    case ERR_NOFOR:
        print("no for");
        break;
    case ERR_NOWHILE:
        print("no while");
        break;
    case ERR_NOSUB:
        print("no sub");
        break;
    case ERR_STACK:
        print("stack");
        break;
    case ERR_COMPLEX:
        print("expr too complex");
        break;
    case ERR_VAR:
        print("variable name expected");
        break;
    case ERR_REDEF:
        print("variable redefined");
        break;
    case ERR_EXPECT:
        print("expected ");
        break;
    case ERR_EXTRA:
        print("unexpected extra");
        break;
    case ERR_DIM:
        print("bad dimension");
        break;
    case ERR_SUBSCR:
        print("bad subscript");
        break;
    case ERR_RUNSUB:
        print("ran into sub");
        break;
    case ERR_STR:
        print("invalid string");
        break;
    case ERR_FILE:
        print("file");
        break;
    case ERR_LINE:
        print("invalid line num");
        break;
    case ERR_EXPR:
        print("invalid expr");
        break;
    case ERR_NUM:
        print("invalid number");
        break;
    case ERR_ARG:
        print("argument");
        break;
    case ERR_TYPE:
        print("type");
        break;
    case ERR_DIVZERO:
        print("div by zero");
        break;
    default:
        print("unknown");
    }
}

/*
 * Based on C's precedence rules.  Higher return value means higher precedence.
 * Ref: http://en.cppreference.com/w/c/language/operator_precedence
 */
unsigned char getprecedence(int token)
{
    switch (token) {
    case TOK_UNP:
    case TOK_UNM:
    case TOK_STAR:
    case TOK_CARET:
    case TOK_NOT:
    case TOK_BITNOT:
        return 11;
    case TOK_POW:
    case TOK_DIV:
    case TOK_MUL:
    case TOK_MOD:
        return 10;
    case TOK_ADD:
    case TOK_SUB:
        return 9;
    case TOK_LSH:
    case TOK_RSH:
        return 8;
    case TOK_GT:
    case TOK_GTE:
    case TOK_LT:
    case TOK_LTE:
        return 7;
    case TOK_EQL:
    case TOK_NEQL:
        return 6;
    case TOK_BITAND:
        return 5;
    case TOK_BITXOR:
        return 4;
    case TOK_BITOR:
        return 3;
    case TOK_AND:
        return 2;
    case TOK_OR:
        return 1;
    case SENTINEL:
        return 0;               /* Must be lowest precedence! */
    default:
        /* Should never happen */
        EXIT(99);
    }
}

/*
 * Operator stack routines
 */

void push_operator_stack(unsigned char operator)
{
    operator_stack[operatorSP] = operator;
    if (!operatorSP) {
        /* Warm start */
        error(ERR_COMPLEX);
        longjmp(jumpbuf, 1);
    }
    --operatorSP;
}

unsigned char pop_operator_stack()
{
    if (operatorSP == STACKSZ - 1) {
        /* Warm start */
        longjmp(jumpbuf, 1);
    }
    ++operatorSP;
    return operator_stack[operatorSP];
}

#define top_operator_stack() operator_stack[operatorSP + 1]

/*
 * Operand stack routines
 */

void push_operand_stack(int operand)
{
    if (compile) {
        emitldi(operand, 0);
        return;
    }
    operand_stack[operandSP] = operand;
    if (!operandSP) {
        /* Warm start */
        error(ERR_COMPLEX);
        longjmp(jumpbuf, 1);
    }
    --operandSP;
}

int pop_operand_stack()
{
    if (compile) {
        return 0;
    }
    if (operandSP == STACKSZ - 1) {
        /* Warm start */
        longjmp(jumpbuf, 1);
    }
    ++operandSP;
    return operand_stack[operandSP];
}

#define top_operand_stack() operand_stack[operandSP + 1]

/*
 ***************************************************************************
 * Parser proper ...
 ***************************************************************************
 */

/*
 * Binary operators
 *
 * Examines the character at *txtPtr, and if not NULL the character at
 * *(txtPtr+1).  If a two character binary operator is found, return the
 * token, otherwise if a one character binary operator is found return
 * the token.  If neither of these, return ILLEGAL.
 *
 * Uses the global tables binaryops (for single char operators) and
 * binaryops1 / binaryops2 (for two character operators).
 */
unsigned char binary()
{
    unsigned char tok;
    unsigned char idx = 0;

/*
 * First two char ops (don't try if first char is NULL though!)
 */

    if (*txtPtr) {
        tok = TOK_EQL;
        while (binaryops1[idx]) {
            if (binaryops1[idx] == *txtPtr) {
                if (binaryops2[idx] == *(txtPtr + 1)) {
                    return tok;
                }
            }
            ++idx;
            ++tok;
        }
    }

/*
 * Now single char ops
 */
    idx = 0;
    tok = TOK_POW;
    while (binaryops[idx]) {
        if (binaryops[idx] == *txtPtr) {
            return tok;
        }
        ++idx;
        ++tok;
    }

    return ILLEGAL;
}

/*
 * Unary operators
 *
 * Examines the character at *txtPtr.  If it is one of the unary operators
 * (which are all single character), returns the token value, otherwise
 * returns ILLEGAL.
 *
 * Uses the global tables unaryops (for single char operators).
 */
unsigned char unary()
{
    unsigned char idx = 0;
    unsigned char tok = TOK_UNM;

    while (unaryops[idx]) {
        if (unaryops[idx] == *txtPtr) {
            return tok;
        }
        ++idx;
        ++tok;
    }
    return ILLEGAL;
}

/*
 * Pop an operator from the operator stack, pop the operands from the
 * operand stack and apply the operator to the operands.
 * Returns 0 if successful, 1 on error
 */
unsigned char pop_operator()
{
    int operand2;
    int result;
    int token = pop_operator_stack();
    int operand1 = pop_operand_stack();

    if (!ISUNARY(token)) {

        /*
         * Evaluate binary operator
         * (Apply the operator token to operand1, operand2)
         */
        operand2 = pop_operand_stack();

        switch (token) {
        case TOK_POW:
            result = _pow(operand2, operand1);
            break;
        case TOK_MUL:
            if (compile) {
                emit(VM_MUL);
                return 0;
            }
            result = operand2 * operand1;
            break;
        case TOK_DIV:
            if (compile) {
                emit(VM_DIV);
                return 0;
            }
            if (operand1 == 0) {
                error(ERR_DIVZERO);
                return 1;
            } else {
                result = operand2 / operand1;
            }
            break;
        case TOK_MOD:
            if (compile) {
                emit(VM_MOD);
                return 0;
            }
            if (operand1 == 0) {
                error(ERR_DIVZERO);
                return 1;
            } else {
                result = operand2 % operand1;
            }
            break;
        case TOK_ADD:
            if (compile) {
                emit(VM_ADD);
                return 0;
            }
            result = operand2 + operand1;
            break;
        case TOK_SUB:
            if (compile) {
                emit(VM_SUB);
                return 0;
            }
            result = operand2 - operand1;
            break;
        case TOK_GT:
            if (compile) {
                emit(VM_GT);
                return 0;
            }
            result = operand2 > operand1;
            break;
        case TOK_GTE:
            if (compile) {
                emit(VM_GTE);
                return 0;
            }
            result = operand2 >= operand1;
            break;
        case TOK_LT:
            if (compile) {
                emit(VM_LT);
                return 0;
            }
            result = operand2 < operand1;
            break;
        case TOK_LTE:
            if (compile) {
                emit(VM_LTE);
                return 0;
            }
            result = operand2 <= operand1;
            break;
        case TOK_EQL:
            if (compile) {
                emit(VM_EQL);
                return 0;
            }
            result = operand2 == operand1;
            break;
        case TOK_NEQL:
            if (compile) {
                emit(VM_NEQL);
                return 0;
            }
            result = operand2 != operand1;
            break;
        case TOK_AND:
            if (compile) {
                emit(VM_AND);
                return 0;
            }
            result = operand2 && operand1;
            break;
        case TOK_OR:
            if (compile) {
                emit(VM_OR);
                return 0;
            }
            result = operand2 || operand1;
            break;
        case TOK_BITAND:
            if (compile) {
                emit(VM_BITAND);
                return 0;
            }
            result = operand2 & operand1;
            break;
        case TOK_BITOR:
            if (compile) {
                emit(VM_BITOR);
                return 0;
            }
            result = operand2 | operand1;
            break;
        case TOK_BITXOR:
            if (compile) {
                emit(VM_BITXOR);
                return 0;
            }
            result = operand2 ^ operand1;
            break;
        case TOK_LSH:
            if (compile) {
                emit(VM_LSH);
                return 0;
            }
            result = operand2 << operand1;
            break;
        case TOK_RSH:
            if (compile) {
                emit(VM_RSH);
                return 0;
            }
            result = operand2 >> operand1;
            break;
        default:
            /* Should never happen */
            EXIT(99);
        }

    } else {

        /*
         * Evaluate unary operator
         * (Apply the operator token to operand1)
         */

        switch (token) {
        case TOK_UNM:
            if (compile) {
                emit(VM_NEG);
                return 0;
            }
            result = -operand1;
            break;
        case TOK_UNP:
            if (compile) {
                return 0;
            }
            result = operand1;
            break;
        case TOK_NOT:
            if (compile) {
                emit(VM_NOT);
                return 0;
            }
            result = !operand1;
            break;
        case TOK_BITNOT:
            if (compile) {
                emit(VM_BITNOT);
                return 0;
            }
            result = ~operand1;
            break;
        case TOK_STAR:
            if (compile) {
                emit(VM_LDAWORD);
                return 0;
            }
            result = *((int *) operand1);
            break;
        case TOK_CARET:
            if (compile) {
                emit(VM_LDABYTE);
                return 0;
            }
            result = *((unsigned char *) operand1);
            break;
        default:
            /* Should never happen */
            EXIT(99);
        }
    }
    push_operand_stack(result);
    return 0;
}

/*
 * Returns 0 if successful, 1 on error
 */
unsigned char push_operator(int operator_token)
{
/* Handles operator precedence here */
    while (getprecedence(top_operator_stack()) >=
           getprecedence(operator_token)) {

        if (pop_operator()) {
            return 1;
        }
    }
    push_operator_stack(operator_token);
    return 0;
}


#define CALLFRAME   0xfffe      /* Magic number for CALL stack frame           */
#define IFFRAME     0xfffd      /* Magic number for IF stack frame             */
#define FORFRAME_B  0xfffc      /* Magic number for FOR stack frame - byte var */
#define FORFRAME_W  0xfffb      /* Magic number for FOR stack frame - word var */
#define WHILEFRAME  0xfffa      /* Magic number for WHILE stack frame          */

/*
 * Push line number (or other int) to return stack.
 */
void push_return(int linenum)
{
#ifdef __GNUC__
//printf("push_return(%d) SP=%d before\n", linenum, returnSP);
#endif
    return_stack[returnSP] = linenum;
    if (!returnSP) {
        error(ERR_STACK);
        longjmp(jumpbuf, 1);
    }
    --returnSP;
}

/*
 * Pop line number (or other int) from return stack.
 */
int pop_return()
{
    if (returnSP == RETSTACKSZ - 1) {
        error(ERR_STACK);
        longjmp(jumpbuf, 1);
    }
    ++returnSP;
#ifdef __GNUC__
//printf("pop_return() ->%d\n", return_stack[returnSP]);
#endif
    return return_stack[returnSP];
}

/*
 * Consume any space characters at txtPtr
 * Macro so it inlines on 6502 for speed.
 */
#define eatspace() \
while (*txtPtr == ' ') { \
	++txtPtr; \
}

/*
 * Returns 0 on success, 1 if error
 * This is only ever invoked for single character tokens
 * (which allows some simplification)
 */
unsigned char expect(unsigned char token)
{

    if (*txtPtr == token) {
        ++txtPtr;               // expect() only called for one char tokens
        eatspace();
        return 0;
    } else {
        error(ERR_EXPECT);
        printchar(token);
        return 1;
    }
}

/*
 * Handles an expression
 * Returns 0 on success, 1 on error
 */
unsigned char E()
{
    int op;

    if (P()) {
        return 1;
    }

    while ((op = binary()) != ILLEGAL) {
        if (push_operator(op)) {
            return 1;
        }
        if (IS1CHBINARY(op)) {
            ++txtPtr;
        } else {
            txtPtr += 2;
        }
        if (P()) {
            return 1;
        }
    }

    while (top_operator_stack() != SENTINEL) {
        if (pop_operator()) {
            return 1;
        }
    }
    return 0;
}

/*
 * Parse array subscript
 * Returns 0 if '[expr]' is found, 1 otherwise
 */
unsigned char subscript(int *idx)
{
/* Start a new subexpression */
    push_operator_stack(SENTINEL);

    if (expect('[')) {
        return 1;
    }
    if (eval(0, idx)) {
        return 1;
    }
    if (expect(']')) {
        return 1;
    }

/* Throw away SENTINEL */
    pop_operator_stack();

    return 0;
}

/*
 * Parse array dimension.  Must be a literal value.
 */
unsigned char dimension(int *idx)
{
    if (expect('[')) {
        return 1;
    }
    if (parseint(idx)) {
        error(ERR_NUM);
        return 1;
    }
    if (expect(']')) {
        return 1;
    }
    return 0;
}

/*
 * Handles a predicate
 * Returns 0 on success, 1 on error
 */
unsigned char P()
{
    struct lineofcode *oldcurrent;
    int oldcounter;
    char key[5];
    int idx;
    char *writePtr;
    unsigned char addressmode;  /* Set to 1 if there is '&' */
    int arg = 0;
    unsigned char type;

    eatspace();

    if (!(*txtPtr)) {
        error(ERR_SYNTAX);      /* This may not really be correct error? */
        return 1;
    }

    if ((*txtPtr == '&') || (isalphach(*txtPtr))) {

        addressmode = 0;

        /*
         * Handle address-of operator
         */
        if (*txtPtr == '&') {
            addressmode = 1;
            ++txtPtr;
            if (!isalphach(*txtPtr)) {
                error(ERR_VAR);
                return 1;
            }
        }

        /*
         * Handle variables
         */
        writePtr = readbuf;
        while (isalphach(*txtPtr) || isdigitch(*txtPtr)) {
            if (arg < 4) {
                key[arg++] = *txtPtr;
            }
            *writePtr = *txtPtr;
            ++txtPtr;
            ++writePtr;
        }
        key[arg] = '\0';
        *writePtr = '\0';

        idx = -1;
        if (*txtPtr == '[') {
            idx = 0;
            if (subscript(&idx) == 1) {
                error(ERR_SUBSCR);
                return 1;
            }

        } else if (*txtPtr == '(') {

            /* No taking address of functions thank you! */
            if (addressmode) {
                error(ERR_VAR);
                return 1;
            }

	    if (compile) {
	        if (docall()) {
		    return 1;
	        }
                goto skip_var;      // MESSY!!
	    } else {

                push_operator_stack(SENTINEL);

                oldcurrent = current;
                oldcounter = counter;

                /*
                 * For CALL, stack frame is just the
                 * magic number, the return line (-2 in this case)
                 * and the txtPtr pointer (-1 again).
                 *
                 * We create this fake CALLFRAME so that the call to 
                 * run() below terminates after hitting a return
                 * statement in the sub being called.
                 */
                push_return(CALLFRAME);
                push_return(-2);    /* Magic number for function */
                push_return(-1);

                /*
                 * Function call - sets up current, counter and txtPtr
                 * to first line of subroutine being called.
                 */
                if (docall()) {
                    return 1;
                }

                /*
                 * Run the function.  When the function returns it 
                 * is treated as immediate mode, so it comes back
                 * here.  txtPtr is restored to immediately after
                 * the call automatically.
                 */
                run(1);

                current = oldcurrent;
                counter = oldcounter;
                /*
                 * Throw away our CALLFRAME.
                 */
                pop_return();
                pop_return();
                pop_return();

                /* Throw away the sentinel */
                pop_operator_stack();

                push_operand_stack(retregister);
                goto skip_var;      // MESSY!!
            }
	}

        if (getintvar(key, idx, &arg, &type, addressmode)) {
            error(ERR_VAR);
            return 1;
        }
        if (!compile) {
            push_operand_stack(arg);
        }

      skip_var:
        eatspace();

    } else if (isdigitch(*txtPtr)) {
        /*
         * Handle integer constants
         */
        if (parseint(&arg)) {
            error(ERR_NUM);
            return 1;
        }
        push_operand_stack(arg);
        eatspace();

    } else if (*txtPtr == '$') {
        /*
         * Handle hex constants
         */
        ++txtPtr;               /* Eat the $ */
        if (parsehexint(&arg)) {
            error(ERR_NUM);
            return 1;
        }
        push_operand_stack(arg);
        eatspace();

    } else if (*txtPtr == '(') {
        /*
         * Handle subexpressions in parenthesis
         */
        ++txtPtr;
        push_operator_stack(SENTINEL);
        if (E()) {
            return 1;
        }
        if (expect(')')) {
            return 1;
        }
        pop_operator_stack();

    } else if ((arg = unary()) != ILLEGAL) {
        /*
         * Handle unary operator
         */
        push_operator_stack(arg);
        ++txtPtr;
        if (P()) {
            return 1;
        }

    } else {
        /*
         * Otherwise error
         */
        error(ERR_EXTRA);
        printchar(' ');
        printchar(*txtPtr);
        return 1;
    }
    return 0;
}

/*
 * Evaluate expression at txtPtr
 * If checkNoMore is 1 then check there is no extra input to be consumed.
 * eval() is basically a wrapper around the expression parser routine E().
 * Result is returned via argument val.
 * Returns 0 if successful, 1 on error.
 */
unsigned char eval(unsigned char checkNoMore, int *val)
{

    eatspace();

    if (!(*txtPtr)) {
        error(ERR_EXPR);
        return 1;
    }
    if (E()) {
        return 1;
    }
    if (checkNoMore == 1) {
        if (*txtPtr == ';') {
            goto doret;
        }

        if (*txtPtr) {
            error(ERR_EXTRA);
            printchar(' ');
            print(txtPtr);
            return 1;
        }
    }
  doret:
    *val = pop_operand_stack();
    return 0;
}

/*
 * Everything above this line is the expression parser.
 * Everything below is the rest of the language implementation.
 */

unsigned char *heap1Ptr;        /* BLK5 heap */
unsigned char *heap2Ptr;        /* BLK3 heap */

#ifdef A2E

/*
 * Apple II Enhanced
 *
 * Code starts at 0x2000.  Top of memory is 0xbfff.
 * Stack is 2K immediately below 0xbfff. (0xb800-0xbfff)
 *
 * Space below 0x2000 is used for file buffers (cc65 provides a module we
 * link in for this.)
 *
 * Heap usage:
 *   Heap 1: Variables
 *   Heap 2: Program text
 */
#define HEAP1TOP (char*)0xb7ff
#define HEAP1LIM (char*)0x9800

#define HEAP2TOP (char*)0x97ff
#define HEAP2LIM (char*)0x8200

#elif defined(C64)

/*
 * C64
 *
 * Here we have a continuous block of RAM from the top of the executable
 * to 0xbfff.  I retain the heap 1 / heap 2 convention of the VIC-20 for now.
 * For now I assign 8K to heap 1 and whatever is left for heap 2.
 *
 * Heap usage:
 *   Heap 1: Variables
 *   Heap 2: Program text
 */
#define HEAP1TOP (char*)0xbfff
#define HEAP1LIM (char*)0xa000

#define HEAP2TOP (char*)0x9fff - 0x0400 /* Leave $400 for the C stack */
#define HEAP2LIM (char*)0x6800  /* HEAP2LIM HAS TO BE ADJUSTED TO NOT
                                 * TRASH THE CODE, WHICH LOADS FROM $0800 UP
                                 * USE THE MAPFILE! */


#elif defined(VIC20)

/*
 * VIC-20:
 *
 * We have two heaps because we have two discontinuous blocks of free memory
 * Heap 1: one using all of BLK5 (8KB)
 * Heap 2: growing down from the top of BLK3 (27.5K less size of executable)
 * The executable is around 19K at the time of writing.
 *
 * Heap usage:
 *   Heap 1: Variables
 *   Heap 2: Program text
 */
#define HEAP1TOP (char*)0xbfff
#define HEAP1LIM (char*)0xa000

#define HEAP2TOP (char*)0x7fff - 0x0400 /* Leave $400 for the C stack */
#define HEAP2LIM (char*)0x7200  /* HEAP2LIM HAS TO BE ADJUSTED TO NOT
                                 * TRASH THE CODE, WHICH LOADS FROM $1200 UP
                                 * USE THE MAPFILE! */

#endif

#ifdef __GNUC__

#define HEAP1SZ 1024*16
unsigned char heap1[HEAP1SZ];
#define HEAP1TOP (heap1 + HEAP1SZ - 1)
#define HEAP1LIM heap1

#endif

/*
 * When compiling, generated code will be stored from start of HEAP 1.
 * Symbol tables grow down from the top of HEAP1
 */
#define CODESTART      HEAP1LIM

/*
 * Clears heap 1.  Must call this before using alloc1().
 */
#define CLEARHEAP1() heap1Ptr = HEAP1TOP

/*
 * Clears heap 2.  Must call this before using alloc2().
 */
#ifdef CC65
#define CLEARHEAP2() heap2Ptr = HEAP2TOP
#endif

/*
 * Clears runtime call stack (target system when compiling)
 * Called before compilation begins.
 */
#define CLEARRTCALLSTACK() rtSP = RTCALLSTACKTOP; rtFP = rtSP; rtPC = RTPCSTART; codeptr = CODESTART;

/*
 * Allocate bytes on heap 1.
 */
void *alloc1(unsigned int bytes)
{
    if ((heap1Ptr - bytes) < HEAP1LIM) {
        print("No mem!\n");
        longjmp(jumpbuf, 1);
    }
    heap1Ptr -= bytes;
    return heap1Ptr;
}

/*
 * Free bytes on heap 1.
 */
void free1(unsigned int bytes)
{
    heap1Ptr += bytes;
}

/*
 * Allocate bytes on target's call stack
 * Starts at RTCALLSTACKTOP and grows down.
 *
 * To have this track the VM's stack pointer, be sure
 * to emit stack push instuctions (VM_PSHWORD / VM_PSHBYTE)
 * instructions that match calls to
 * rt_push_callstack().
 */
int rt_push_callstack(unsigned int bytes)
{
    if ((rtSP - bytes) < RTCALLSTACKLIM) {
        print("No tgt mem!\n");
        longjmp(jumpbuf, 1);
    }
    rtSP -= bytes;
    return rtSP;
}

/*
 * Free bytes on target's call stack
 *
 * To have this track the VM's stack pointer, be sure
 * to emit VM_POPWORD / VM_POPBYTE / VM_RTS instructions
 * that match calls to rt_pop_callstack().
 *
 * Note that local variables are freed on function end
 * using the FPSP instruction.
 */
void rt_pop_callstack(unsigned int bytes)
{
    rtSP += bytes;
}

/*
 * Allocate bytes on heap 2.
 */
void *alloc2(unsigned int bytes)
{
#ifdef __GNUC__
    void *p = malloc(bytes);
    if (!p) {
        print("No mem!\n");
        longjmp(jumpbuf, 1);
    }
    return p;
#else
    if ((heap2Ptr - bytes) < HEAP2LIM) {
        print("No mem!\n");
        longjmp(jumpbuf, 1);
    }
    heap2Ptr -= bytes;
    return heap2Ptr;
#endif
}

/*
 * Return the total amount of free space on heap 1.
 */
int getfreespace1()
{
    return (heap1Ptr - HEAP1LIM + 1);
}

/*
 * Return the total amount of free space on heap 2.
 */
#ifdef CC65
int getfreespace2()
{
    return (heap2Ptr - HEAP2LIM + 1);
}
#endif

/*
 * Compiler: Emit bytecode (used for everything except VM_LDIMM)
 * Stores using codeptr.
 */
void emit(enum bytecode code)
{
    unsigned char c = code;

    *codeptr = code;
    ++codeptr;

    printhex(rtPC);
    print(": ");
    printhexbyte(c);
    print("       : ");
    print(bytecodenames[c]);
    printchar('\n');
    ++rtPC;
}

/*
 * Compiler: Emit word argument (VM_LDIMM opcode)
 * Stores using codeptr.
 */
void emitldi(int word, char *comment)
{
    unsigned char c = VM_LDIMM;
    unsigned char *p = (unsigned char *) &word;

    *codeptr = c;
    ++codeptr;
    *codeptr = *p;
    ++codeptr;
    ++p;
    *codeptr = *p;
    ++codeptr;

    printhex(rtPC);
    print(": ");
    printhexbyte(c);
    printchar(' ');
    printhex(word);
    print(" : ");
    print(bytecodenames[c]);
    printchar(' ');
    printhex(word);
    if (comment) {
        print(" ; ");
        print(comment);
    }
    printchar('\n');
    rtPC += 3;
}

/*
 * Compiler: Emit PRMSG and string argument.
 * String is in readbuf
 * String is zero-terminated.
 */
void emitprmsg(void)
{
    char *p = readbuf;
    emit(VM_PRMSG);
    ++rtPC;
    printchar('"');
    while (*p) {
        *codeptr = *p;
	++rtPC;
	printchar(*p);
	++codeptr;
	++p;
    }
    *codeptr = 0;
    ++codeptr;

    print("\"\n");
}

/*
 * Emit fixup for address.
 * The compiler uses this to go back and fill in the address for forward
 * jumps, once it discovers where the destination is.
 */
void emit_fixup(int address, int word)
{
    unsigned char *ptr = (unsigned char *)(CODESTART + address - RTPCSTART);
    unsigned char *p = (unsigned char *) &word;

    *ptr = *p;
    ++ptr;
    ++p;
    *ptr = *p;

    printhex(address);
    print(": ");
    printhex(word);
    print("                 ; Fixup\n");
}

/*
 * Write code to file.
 * Call this after compilation is done.
 */
void writebytecode()
{
    unsigned char *end = codeptr;
    unsigned char *p;
    p = (unsigned char *) CODESTART;
    strcpy(readbuf, "bytecode");
    openfile(1);
    print("...\n");
    while (p < end) {
        //printhexbyte(*p);
        //print("\n");
#ifdef CBM
        cbm_write(1, p, 1);
#else
        fwrite(p, 1, 1, fd);
#endif
        ++p;
    }
#ifdef CBM
    cbm_close(1);
#else
    fclose(fd);
#endif
}

/*
 * Values:
 *  0 not editing program
 *  1 editing program
 *  2 editing program - insert first line
 */
unsigned char editmode = 0;

/*
 * Pointer to first line of code.
 */
struct lineofcode *program = NULL;

/**
 * skipFlag is set to one when we enter a body of code which we are not
 * executing (for example because a while loop condition was false.)  When
 * skipFlag is one, the parser will only process certain loop control tokens
 * - all others are ignored.
 */
unsigned char skipFlag;

/*
 * Append a line to the program
 * If current is set, then the new line will be appended after current
 * and current will be moved forward to point to the newly added line.
 * If current is NULL, then this is the first line of a new program
 */
void appendline(char *line)
{
    struct lineofcode *loc = alloc2(sizeof(struct lineofcode));

    loc->line = alloc2(sizeof(char) * (strlen(line) + 1));
    strcpy(loc->line, line);
    if (!current) {
        loc->next = NULL;
        new();
        program = current = loc;
    } else {
        loc->next = current->next;
        current->next = loc;
        current = loc;
    }
}

/*
 * Insert new first line (special case)
 */
void insertfirstline(char *line)
{
    struct lineofcode *loc = alloc2(sizeof(struct lineofcode));

    loc->line = alloc2(sizeof(char) * (strlen(line) + 1));
    strcpy(loc->line, line);
    loc->next = program;
    program = loc;
}

/*
 * Make current point to the line with number linenum
 * (or NULL if not found).
 * Line numbers start from one in this routine.
 */
void findline(int linenum)
{
    counter = 1;
    current = program;
    while (current) {
        if (counter == linenum) {
            return;
        }
        current = current->next;
        ++counter;
    }
}

/*
 * Delete line(s)
 */
void deleteline(int startline, int endline)
{
    int linesToDel = endline - startline + 1;
    struct lineofcode *prev = NULL;

    counter = 1;
    if (endline < startline) {
        return;
    }
    current = program;
    while (current && linesToDel) {
        if (counter == startline) {
            if (prev) {
                prev->next = current->next;
            } else {
                program = current->next;
            }
#ifdef __GNUC__
            free(current->line);
            free(current);
#endif
            current = current->next;    /* ILLEGAL BUT WORKS FOR NOW */
            --linesToDel;
            continue;
        }
        prev = current;
        current = current->next;
        ++counter;
    }
}

/*
 * Replace line pointed to by current
 */
void changeline(char *line)
{
#ifdef __GNUC__
    free(current->line);
#endif
    current->line = alloc2(sizeof(char) * (strlen(line) + 1));
    strcpy(current->line, line);
}

/*
 * Delete program, free memory.
 */
void new()
{
#ifdef __GNUC__
    struct lineofcode *l = program;
    struct lineofcode *l2;

    while (l) {
        l2 = l->next;
        free(l->line);
        free(l);
        l = l2;
    }
#else
    /* No need to iterate and free them all, just dump the heap */
    CLEARHEAP2();
#endif
    program = NULL;
}

/* 0 is the top level, 1 is first level sub call etc. */
int calllevel;

/*
 * Entry in the variable table
 * name: first 4 characters as key
 * type: encodes the type in the least significant 4 bits (TYPE_WORD or
 *       TYPE_BYTE).  The most significant 4 bits are zero for scalars and
 *       encode the number of dimensions in the case of an array.
 * next: pointer to next vartabent.
 */
struct vartabent {
    char name[4];
    unsigned char type;         /* See above */
    struct vartabent *next;
};

typedef struct vartabent var_t;

var_t *varsbegin;               /* First table entry */
var_t *varsend;                 /* Last table entry  */
var_t *varslocal;               /* Local stack frame */

/*
 * Entry in the subroutine table.  This is used by the compiler only.
 * name: first 8 characters as key
 * addr: address of entry point in compiled code.
 */
struct subtabent {
	char name[8];
	unsigned int addr;
	struct subtabent *next;
};

typedef struct subtabent sub_t;

sub_t *subsbegin;  /* Entry points of compiled subroutines - first */
sub_t *subsend;    /* Entry points of compiled subroutines - last  */
sub_t *callsbegin; /* Subroutine calls - first */
sub_t *callsend;   /* Subroutine calls - end */

#define getptrtoscalarword(v) (int*)((char*)v + sizeof(var_t))
#define getptrtoscalarbyte(v) (unsigned char*)((char*)v + sizeof(var_t))

/*
 * Find integer variable
 * local - pointer to unsigned char.  If this contains 1 on entry then
 * only local variables will be searched.  The value returned in this
 * field can be used to determine if variable found was local (1) or
 * global (0).
 */
var_t *findintvar(char *name, unsigned char *local)
{
    var_t *ptr;

    //getvarkey(name, key);

    /* Search locals */
    ptr = varslocal;
    while (ptr) {
        if (!strncmp(name, ptr->name, 4)) {
            *local = 1;
            return ptr;
        }
        ptr = ptr->next;
    }

    if (*local == 1) {
        return NULL;
    }

    /* Search globals */
    ptr = varsbegin;
    while (ptr && (ptr->name[0] != '-')) {
        if (!strncmp(name, ptr->name, 4)) {
            *local = 0;
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;                /* Not found */
}

/*
 * Clear all variables
 */
void clearvars()
{
/* No need to iterate and free them all, just dump the heap */
    CLEARHEAP1();
    varsbegin = NULL;
    varsend = NULL;
    varslocal = NULL;
}

enum types { TYPE_WORD,         /* Word variable - 16 bits */
    TYPE_BYTE
};                              /* Byte variable - 8 bits  */

/*
 * Print all variables as a table
 */
void printvars()
{
    var_t *v = varsbegin;

    while (v) {
        printchar(v->name[0] ? v->name[0] : ' ');
        printchar(v->name[1] ? v->name[1] : ' ');
        printchar(v->name[2] ? v->name[2] : ' ');
        printchar(v->name[3] ? v->name[3] : ' ');
        if (v->type & 0xf0) {
            printchar('[');
            printdec(*(int *)
                     ((unsigned char *) v + sizeof(var_t) + sizeof(int)));
            printchar(']');
        }
        printchar(' ');
        if ((v->type & 0x0f) == TYPE_WORD) {
            printchar('w');
        } else {
            printchar('b');
        }
        printchar(' ');
        if ((v->type & 0xf0) == 0) {
            if (v->type == TYPE_WORD) {
                printdec(*getptrtoscalarword(v));
            } else {
                printdec(*getptrtoscalarbyte(v));
            }
        }
        printchar('\n');
        v = v->next;
    }
}

/*
 * Create new integer variable (either word or byte, scalar or array)
 *
 * name is the variable name
 * type specifies if it is a word or byte variable
 * numdims in the number of dimensions (0 for scalar, >=1 for array)
 * sz is the size - ***  TODO Only supports 1D arrays for now
 * value is the initializer
 * bodyptr is used when allocating arrays.  If bodyptr is null then the
 * function will allocate space for the array data block, following the
 * array header.  If, on the other hand, a non-null pointer is passed then
 * only the array header will be allocated and the pointer will be stored
 * as the pointer to the array data block.  This allows array pass-by-reference
 * semantics.
 * 
 * Return 0 on success, 1 if error.
 *
 * New variable is appended to table, which adds it to the innermost scope.
 */
unsigned char createintvar(char *name,
                           enum types type,
                           unsigned char numdims,
                           int sz, int value, int bodyptr)
{
    int i;
    var_t *v;
    unsigned char local = 1;

    v = findintvar(name, &local);       /* Only search local scope */

    if (v) {
        error(ERR_REDEF);
        return 1;
    }

    if (sz < 1) {
        error(ERR_DIM);
        return 1;
    }

    if (numdims == 0) {
        /*
         * Scalar variables
         */
        if (compile) {
            /*
             * When compiling we store the address of the variable in
             * the target VM where the value normally goes.
             *
             * For local variables this is RELATIVE to the frame pointer but
             * for globals it is an ABSOLUTE address.
             */
            v = alloc1(sizeof(var_t) + sizeof(int));
            if (type == TYPE_WORD) {
                if (compilingsub) {
                    *getptrtoscalarword(v) = rt_push_callstack(2) - rtFP;       /* Relative to Frame Ptr */
                } else {
                    *getptrtoscalarword(v) = rt_push_callstack(2) + 1;      /* Absolute addr */
                }
                emit(VM_PSHWORD);
            } else {
                if (compilingsub) {
                    *getptrtoscalarword(v) = rt_push_callstack(1) - rtFP;       /* Relative to Frame Ptr */
                } else {
                    *getptrtoscalarword(v) = rt_push_callstack(1) + 1;      /* Absolute addr */
                }
                emit(VM_PSHBYTE);
            }
        } else {
            if (type == TYPE_WORD) {
                v = alloc1(sizeof(var_t) + sizeof(int));
                *getptrtoscalarword(v) = value;
            } else {
                v = alloc1(sizeof(var_t) + sizeof(unsigned char));
                *getptrtoscalarbyte(v) = value;
            }
        }
    } else {
        /*
         * Array variables
         *
         * Here we allocate two words of space as follows:
         *  WORD1: Pointer to payload
         *  WORD2: to record the single dimensions of the 1D array.
         * The payload follows these two words.  This scheme is
         * designed to be extensible to more dimensions.
         */

        if (bodyptr) {

            /*
             * Should work for both interpreter and compiler
             * (although only used in interpreter right now)
             */
            v = alloc1(sizeof(var_t) + 2 * sizeof(int));

        } else {
            if (compile) {

                /* *** Initializer value is on stack (X) *** */
                v = alloc1(sizeof(var_t) + 2 * sizeof(int));
                if (type == TYPE_WORD) {
                    if (compilingsub) {
                        bodyptr = rt_push_callstack(sz * 2) - rtFP;     /* Rel Frame Ptr */
                    } else {
                        bodyptr = rt_push_callstack(sz * 2) + 1;    /* Absolute */
                    }
                } else {
                    if (compilingsub) {
                        bodyptr = rt_push_callstack(sz) - rtFP; /* Rel Frame Ptr */
                    } else {
                        bodyptr = rt_push_callstack(sz) + 1;        /* Absolute */
                    }
                }

                /*
                 * The following generates code to initialize the array
                 */
                emitldi(sz, "sz");
                emit(VM_DEC);
                emit(VM_DUP);
                emitldi(3, 0);
                emit(VM_PICK);
                if (type == TYPE_WORD) {
                    emit(VM_PSHWORD);
                } else {
                    emit(VM_PSHBYTE);
                }
                emitldi(0, 0);
                emit(VM_NEQL);
                emitldi(rtPC - 11, "loop");
                emit(VM_BRNCH);
            } else {
                if (type == TYPE_WORD) {
                    v = alloc1(sizeof(var_t) + (sz + 2) * sizeof(int));
                    bodyptr = (int) ((unsigned char *) v + sizeof(var_t) + 2 * sizeof(int));

                    /* Initialize array */
                    for (i = 0; i < sz; ++i) {
                        *((int *) bodyptr + i) = value;
                    }
                } else {
                    v = alloc1(sizeof(var_t) + 2 * sizeof(int) + sz * sizeof(unsigned char));
                    bodyptr = (int) ((unsigned char *) v + sizeof(var_t) + 2 * sizeof(int));

                    /* Initialize array */
                    for (i = 0; i < sz; ++i) {
                        *((unsigned char *) bodyptr + i) = value;
                    }
                }
            }
        }

        /* Store pointer to payload */
        *(int *) ((unsigned char *) v + sizeof(var_t)) = (int) bodyptr;

        /* Store size */
        *(int *) ((unsigned char *) v + sizeof(var_t) + sizeof(int)) = sz;
    }

    strncpy(v->name, name, 4);
    v->type = ((numdims & 0x0f) << 4) | type;
    v->next = NULL;

    if (varsend) {
        varsend->next = v;
    }
    varsend = v;
    if (!varsbegin) {
        varsbegin = v;
        varslocal = v;
    }
    return 0;
}

/*
 * Mark variable table when we enter a subroutine.
 * We use a fake variable entry for this, with illegal name '----'
 * Records pointer to the fake entry in varslocal.
 */
void vars_markcallframe()
{
    ++calllevel;
    varslocal = alloc1(sizeof(var_t) + sizeof(int));
    strncpy(varslocal->name, "----", 4);
    varslocal->type = TYPE_WORD;
    varslocal->next = NULL;
    *(getptrtoscalarword(varslocal)) = (int) varsend;   /* Store pointer to previous in value */
    if (varsend) {
        varsend->next = varslocal;
    }
    varsend = varslocal;
    if (!varsbegin) {
        varsbegin = varslocal;
    }
}

/*
 * Release local variables on return from a subroutine.
 */
void vars_deletecallframe()
{
    var_t *newend = (void *) *(getptrtoscalarword(varslocal));  /* Recover pointer */
    var_t *v = varslocal;

    /* Free the local variables */
    if (!newend) {
        CLEARHEAP1();
    } else {
        free1((int) newend - (int) varsend);
    }

    if (newend) {
        newend->next = NULL;
    } else {
        varsbegin = NULL;
    }
    varsend = newend;
    --calllevel;

    /* Set varslocal to previous stack frame or NULL if none */
    varslocal = NULL;
    v = varsbegin;
    while (v) {
        if (v->name[0] == '-') {
            varslocal = v;
        }
        v = v->next;
    }
}

/*
 * Set existing integer variable
 * name is the variable name
 * idx is the index into an array. -1 means subscript not given.
 * value is the value to set
 * Return 0 if successful, 1 on error
 *
 * Sets matching local variable.  If no local exists then return the
 * matching global.  Otherwise error.
 */
unsigned char setintvar(char *name, int idx, int value)
{
    unsigned char numdims;
    void *bodyptr;
    unsigned char local = 0;

    var_t *ptr = findintvar(name, &local);

    if (!ptr) {
        error(ERR_VAR);
        return 1;
    }
    numdims = (ptr->type & 0xf0) >> 4;

    if (numdims == 0) {
	/*
	 * Scalars
	 */
        if (idx != -1) {
	    /* Means [..] subscript was provided */
	    error(ERR_SUBSCR);
	    return 1;
	}
        if (compile) {
            /*
             * When we are at the top level scope (global scope), all
             * variables are globals and we use ABSOLUTE addressing.
             * When we are at function scope, globals still use
             * ABSOLUTE addressing, but locals are addressed RELATIVE
             * to the frame pointer.
             */
            emitldi(*getptrtoscalarword(ptr), name);
            if ((ptr->type & 0x0f) == TYPE_WORD) {
                if (local && compilingsub) {
                    emit(VM_STRWORD);
                } else {
                    emit(VM_STAWORD);
                }
            } else {
                if (local && compilingsub) {
                    emit(VM_STRBYTE);
                } else {
                    emit(VM_STABYTE);
                }
            }
        } else {
            if ((ptr->type & 0x0f) == TYPE_WORD) {
                *getptrtoscalarword(ptr) = value;
            } else {
                *getptrtoscalarbyte(ptr) = value;
            }
        }
    } else {
	/*
	 * Arrays
	 */
        if (idx == -1) {
	    /* Means [..] subscript was never provided */
	    error(ERR_SUBSCR);
	    return 1;
	}
        bodyptr =
            (void *) *(int *) ((unsigned char *) ptr + sizeof(var_t));

        if (compile) {
            /* *** Index is on the stack (Y), and initializer is on the stack (X) *** */
            emit(VM_SWAP);
            if ((ptr->type & 0x0f) == TYPE_WORD) {
                emitldi(2, 0);
                emit(VM_MUL);
                emitldi((int) ((int *) bodyptr), name);
                emit(VM_ADD);
                if (local && compilingsub) {
                    emit(VM_STRWORD);
                } else {
                    emit(VM_STAWORD);
                }
            } else {
                emitldi((int) ((int *) bodyptr), name);
                emit(VM_ADD);
                if (local && compilingsub) {
                    emit(VM_STRBYTE);
                } else {
                    emit(VM_STABYTE);
                }
            }
        } else {
            if ((idx < 0) || (idx >=
                *(int *) ((unsigned char *) ptr + sizeof(var_t) +
                          sizeof(int)))) {
                error(ERR_SUBSCR);
                return 1;
            }
            if ((ptr->type & 0x0f) == TYPE_WORD) {
                *((int *) bodyptr + idx) = value;
            } else {
                *((unsigned char *) bodyptr + idx) = value;
            }
        }
    }

    return 0;
}

/* Special hack ... */
unsigned char compiletimelookup = 0;

/*
 * Get existing integer variable
 * name is the variable name
 * idx is the index into an array. -1 means subscript not given.
 * Returns the value (or the address) in val.
 * Return the type TYPE_BYTE or TYPE_WORD in type.
 * address if set to 1 then address is returned, not value
 * Return 0 if successful, 1 on error
 *
 * Returns matching local variable.  If no local exists then return the
 * matching global.  Otherwise error.
 */
unsigned char getintvar(char *name,
                        int idx,
                        int *val,
                        unsigned char *type, unsigned char address)
{
    unsigned char numdims;
    void *bodyptr;
    unsigned char local = 0;

    var_t *ptr = findintvar(name, &local);

    if (!ptr) {
        error(ERR_VAR);
        return 1;
    }
    numdims = (ptr->type & 0xf0) >> 4;
    if ((ptr->type & 0x0f) == TYPE_WORD) {
        *type = TYPE_WORD;
    } else {
        *type = TYPE_BYTE;
    }

    if (numdims == 0) {
	/*
	 * Scalars
	 */
        if (idx != -1) {
	    /* Means [..] subscript was provided */
	    error(ERR_SUBSCR);
	    return 1;
	}
        if (compile) {
            if (compiletimelookup) {
                /*
                 * Special hack to allow lookup (rather than code
                 * generation) during compilation.
                 */
                *val = *getptrtoscalarword(ptr);

            } else {
                /*
                 * When we are at the top level scope (global scope), all
                 * variables are globals and we use ABSOLUTE addressing.
                 * When we are at function scope, globals still use
                 * ABSOLUTE addressing, but locals are addressed RELATIVE
                 * to the frame pointer.
                 */
                if (address) {
                    emitldi(*getptrtoscalarword(ptr), name);
                    if (local && compilingsub) {
			emit(VM_RTOA);
                    }
                } else {
                    if ((ptr->type & 0x0f) == TYPE_WORD) {
                        if (local && compilingsub) {
                            emitldi(*getptrtoscalarword(ptr), name);
                            emit(VM_LDRWORD);
                        } else {
                            emitldi(*getptrtoscalarword(ptr), name);
                            emit(VM_LDAWORD);
                        }
                    } else {
                        if (local && compilingsub) {
                            emitldi(*getptrtoscalarword(ptr), name);
                            emit(VM_LDRBYTE);
                        } else {
                            emitldi(*getptrtoscalarword(ptr), name);
                            emit(VM_LDABYTE);
                        }
                    }
                }
            }
        } else {
            if ((ptr->type & 0x0f) == TYPE_WORD) {
                if (address) {
                    *val = (int) getptrtoscalarword(ptr);
                } else {
                    *val = *getptrtoscalarword(ptr);
                }
            } else {
                if (address) {
                    *val = (int) getptrtoscalarbyte(ptr);
                } else {
                    *val = *getptrtoscalarbyte(ptr);
                }
            }
        }
    } else {
	/*
	 * Arrays
	 * Note the special case:
	 * For an array A, &A is the same as &A[0]
	 */
        if (idx == -1) {
	    if (address) {
                idx = 0;
		if (compile) {
		    emitldi(0, 0);
		}
	    } else {
	        /* Means [..] subscript was never provided */
	        error(ERR_SUBSCR);
	        return 1;
	    }
	}
        bodyptr =
            (void *) *(int *) ((unsigned char *) ptr + sizeof(var_t));

        if (compile) {
            /* *** Index is on the stack (X) *** */
            if ((ptr->type & 0x0f) == TYPE_WORD) {
                emitldi(2, 0);
                emit(VM_MUL);
                emitldi((int) ((int *) bodyptr), name);
                emit(VM_ADD);
		if (!address) {
                    if (local && compilingsub) {
                        emit(VM_LDRWORD);
                    } else {
                        emit(VM_LDAWORD);
                    }
		}
            } else {
                emitldi((int) ((int *) bodyptr), name);
                emit(VM_ADD);
		if (!address) {
                    if (local && compilingsub) {
                        emit(VM_LDRBYTE);
                    } else {
                        emit(VM_LDABYTE);
                    }
		}
            }
        } else {
            if ((idx < 0) || (idx >=
                *(int *) ((unsigned char *) ptr + sizeof(var_t) +
                          sizeof(int)))) {
                error(ERR_SUBSCR);
                return 1;
            }

            if ((ptr->type & 0x0f) == TYPE_WORD) {
                if (address) {
                    *val = (int) ((int *) bodyptr + idx);
                } else {
                    *val = *((int *) bodyptr + idx);
                }
            } else {
                if (address) {
                    *val = (int) ((unsigned char *) bodyptr + idx);
                } else {
                    *val = *((unsigned char *) bodyptr + idx);
                }
            }
        }
    }

    return 0;
}


/*
 * Handy defines for return codes
 */
#define RET_SUCCESS      0      /* Successful */
#define RET_ERROR        1      /* Error */

/*************************************************************************/
/* IF / THEN / ELSE                                                      */
/*************************************************************************/

/*
 * Handles if statement.
 */
void doif(unsigned char arg)
{

#ifdef __GNUC__
//print("doif()\n");
#endif

    /*
     * Place the following on the return stack when interpreting:
     *   - Magic value IFFRAME to indicate IF loop stack frame
     *   - Status value as follows:
     *     0: skipFlag was already set so not evaluating my argument
     *     1: skipFlag was clear and I set it (condition false)
     *     2: skipFlag was clear and I left it clear (condition true)
     *   - Dummy value 0
     *
     * When compiling:
     *   - Magic value IFFRAME
     *   - Address of the branch destination operand for when condition
     *     is false.  This will be filled in later and may point to the
     *     ELSE block (if present) or end of the IF block otherwise.
     *   - Space to store the branch destination operand which will be
     *     used to skip over the ELSE block if the IF block runs.  This
     *     will also be filled in later.
     */
    push_return(IFFRAME);

    if (compile) {
        /* **** Value of IF expression is on the VM stack **** */
        emit(VM_NOT);
        push_return(rtPC + 1);  /* Address of dummy 0xffff */
        emitldi(0xffff, "TDB");
        emit(VM_BRNCH);
        push_return(0);
    } else {
        if (skipFlag) {
            push_return(0);
        } else {
            if (!arg) {
                skipFlag = 1;
                push_return(1);
            } else {
                push_return(2);
            }
        }
        push_return(0);         /* Dummy */
    }
}

/*
 * Handles else statement.
 * Returns RET_SUCCESS if no error, RET_ERROR if error
 */
unsigned char doelse()
{

#ifdef __GNUC__
//print("doelse()\n");
#endif

    if (return_stack[returnSP + 3] != IFFRAME) {
        error(ERR_NOIF);
        return RET_ERROR;
    }

    if (compile) {
        /*
         * Code to jump over ELSE block when IF condition is true
         */
        return_stack[returnSP + 1] = rtPC;
        emitldi(0xffff, "TDB");
        emit(VM_JMP);

        /*
         * Fixup the dummy destination address initialized by
         * doif() to point to ELSE
         */
        emit_fixup(return_stack[returnSP + 2], rtPC);
        return_stack[returnSP + 2] = 0;
    } else {
        /*
         * If the matching IF statement had condition true then the
         * value at return_stack[returnSP + 1] will be 2.  If the
         * matching IF statement had condition false then the value
         * will be 1.
         */
        if (return_stack[returnSP + 2] == 2) {
            skipFlag = 1;
        } else if (return_stack[returnSP + 2] == 1) {
            skipFlag = 0;
        }
    }
    return RET_SUCCESS;
}

/*
 * Handles endif statement.
 * Returns RET_SUCCESS if no error, RET_ERROR if error
 */
unsigned char doendif()
{

#ifdef __GNUC__
//print("doendif()\n");
#endif

    if (return_stack[returnSP + 3] != IFFRAME) {
        error(ERR_NOIF);
        return RET_ERROR;
    }

    if (compile) {
        /*
         * Fixup the dummy destination address initialized by
         * doif() to point to ENDIF. (Only do this if it hasn't
         * already been updated to point to the ELSE.)
         */
        if (return_stack[returnSP + 2]) {
            emit_fixup(return_stack[returnSP + 2], rtPC);
        }
        /*
         * Fixup the dummy destination address initialized by
         * doelse() to point to ENDIF. (Only do this if there was
         * actually an ELSE.)
         */
        if (return_stack[returnSP + 1]) {
            emit_fixup(return_stack[returnSP + 1] + 1, rtPC);
        }
    } else {
        /*
         * If skipFlag was false when we hit the matching IF
         * statement, then the value at return_stack[returnSP + 2]
         * will be 1 or 2. In this case, clear skipFlag.
         */
        if (return_stack[returnSP + 2]) {
            skipFlag = 0;
        }
    }

    pop_return();
    pop_return();
    pop_return();
    return RET_SUCCESS;
}

/*************************************************************************/
/* FOR LOOPS                                                             */
/*************************************************************************/

/*
 * Routine handles four cases, each of which looks like variable assignment.
 * Doing these all together here makes code easier to maintain and smaller.
 */
#define WORD_MODE 0
#define BYTE_MODE 1
#define LET_MODE  2
#define FOR_MODE  3

/*
 * Handles four cases, according to value of mode:
 *  - WORD_MODE - creation of word variable 
 *  - BYTE_MODE - creation of byte variable 
 *  - LET_MODE - assignment to existing variable
 *  - FOR_MODE - entry to for loop
 *
 * Handles parsing the following text (formore == 0):
 *     "var = expr"
 *     "var[expr1] = expr2"
 * or (mode == FOR_MODE):
 *     "var = expr2 : expr3"
 * or, "var[expr1] = expr2 : expr3"
 *
 * Returns RET_SUCCESS if no error, RET_ERROR if error
 */
unsigned char assignorcreate(unsigned char mode)
{
    int j;
    int k;
    unsigned char type;
    char name[5];
    int i = 0;
    unsigned char numdims = 0;
    unsigned char local = 0;

#ifdef __GNUC__
//print("assignorcreate()\n");
#endif

    if (!txtPtr || !isalphach(*txtPtr)) {
        error(ERR_VAR);
        return RET_ERROR;
    }
    while (*txtPtr && (isalphach(*txtPtr) || isdigitch(*txtPtr))) {
        if (i < 4) {
            name[i++] = *txtPtr;
        }
        ++txtPtr;
    }
    name[i] = '\0';

    i = 0;
    if (*txtPtr == '[') {
        numdims = 1;
        switch (mode) {
        case WORD_MODE:
        case BYTE_MODE:
            /* Array dimensions must be numeric literal */
            if (dimension(&i) == 1) {
                return RET_ERROR;
            }
            break;
        default:
            if (subscript(&i) == 1) {
                return RET_ERROR;
            }
        }
    }

    eatspace();

    if (expect('=')) {
        return RET_ERROR;
    }

    if (eval((mode != FOR_MODE), &j)) {
        return RET_ERROR;
    }
    switch (mode) {

    case WORD_MODE:
        if (i == 0) {
            ++i;
        }
        if (createintvar(name, TYPE_WORD, numdims, i, j, 0)) {
            return RET_ERROR;
        }
        break;

    case BYTE_MODE:
        if (i == 0) {
            ++i;
        }
        if (createintvar(name, TYPE_BYTE, numdims, i, j, 0)) {
            return RET_ERROR;
        }
        break;

    case LET_MODE:
	if (numdims == 0) {
		i = -1;
	}
        if (setintvar(name, i, j)) {
            return RET_ERROR;
        }
        break;

    case FOR_MODE:
	if (numdims == 0) {
		i = -1;
	}
        if (setintvar(name, i, j)) {
            return RET_ERROR;
        }
        break;
    }

    if (mode != FOR_MODE) {
        return RET_SUCCESS;
    }

    /*
     * The remaining code is to handle entry to FOR
     * mode == FOR_MODE
     */

    if (expect(':')) {
        return RET_ERROR;
    }

    if (eval(1, &k)) {
        return RET_ERROR;
    }

    /*
     * Place the following on the return stack when interpreting:
     * - Magic value FORFRAME_B or FORFRAME_W - to indicate FOR loop stack
     *   frame for byte or word variable respectively.
     * - Line counter for the for statement (here) (int)
     * - txtPtr (int*)
     * - Loop limit (int)
     * - Pointer to loop control variable (int*)
     *
     * When compiling:
     *
     * - Magic value FORFRAME_B or FORFRAME_W.
     * - Runtime PC
     * - Pointer to loop control variable.
     * - Dummy word
     * - Dummy word
     */

    /* Get the address of the variable */
    if (compile) {
        compiletimelookup = 1;
    }
    if (getintvar(name, i, &j, &type, 1)) {
        compiletimelookup = 0;
        error(ERR_VAR);
        return RET_ERROR;
    }
    compiletimelookup = 0;

    if (type == TYPE_WORD) {
        push_return(FORFRAME_W);
    } else {
        push_return(FORFRAME_B);
    }
    if (compile) {
        /* Find out if it is a local or a global */
        findintvar(name, &local);
        if (local && compilingsub) {
            push_return(1);     // Use relative
        } else {
            push_return(0);     // Use absolute
        }
        push_return(rtPC);      /* Store PC for compile case */
	emit(VM_DUP);

        /* Loop limit k should be on the runtime VM stack already */
        push_return(j);
        push_return(0);         /* Dummy */
    } else {
        push_return(counter);
        push_return((int) txtPtr);
        push_return(k);
        push_return(j);
    }

    return RET_SUCCESS;
}

/*
 * Go back to the start of a loop or return after end of subroutine.
 * (Used for FOR and WHILE loops and for subroutine CALL/RETURN).
 * - linenum is the line number of the start of the loop or the call
 *   statement, or -1 in immediate mode.
 * - oldTxtPtr is the stashed text pointer, which is expected to point
 *   to the code immediately after the opening loop statement.
 */
void backtotop(int linenum, char *oldTxtPtr)
{

    if (linenum == -1) {
        /* Return to immediate mode */
        counter = -1;
        current = NULL;
    } else {
        /*
         * If not immediate mode, then reload the line containing
         * the opening statement of the loop, or the call statement
         */
        findline(linenum + 1);
        --counter;              /* Findline uses 1-based linenums */
        if (!current) {
            /* Should never get here! */
            EXIT(99);
        }
    }

    txtPtr = oldTxtPtr;
}

/*
 * Handle iterating or exiting the FOR loop.
 * Expects to find pointer to loop variable, loop limit
 * and line counter on the return stack.
 * Returns RET_SUCCESS on success, RET_ERROR on error
 */
unsigned char doendfor()
{
    int val;
    unsigned char type = 0xff;

#ifdef __GNUC__
//print("doendfor()\n");
#endif

    if (return_stack[returnSP + 5] == FORFRAME_W) {
        type = TYPE_WORD;
        if (!compile) {
            val = *(int *) (return_stack[returnSP + 1]);
        }
    } else if (return_stack[returnSP + 5] == FORFRAME_B) {
        type = TYPE_BYTE;
        if (!compile) {
            val = *(unsigned char *) (return_stack[returnSP + 1]);
        }
    }
    if (type == 0xff) {
        error(ERR_NOFOR);
        return RET_ERROR;
    }

    if (compile) {
        /* **** Loop limit is on the VM stack **** */
        emitldi(return_stack[returnSP + 2], "loop var");
        if (type == TYPE_WORD) {
            if (return_stack[returnSP + 4]) {
                emit(VM_LDRWORD);
            } else {
                emit(VM_LDAWORD);
            }
        } else {
            if (return_stack[returnSP + 4]) {
                emit(VM_LDRBYTE);
            } else {
                emit(VM_LDABYTE);
            }
        }
        emit(VM_INC);
        emit(VM_DUP);
        emitldi(return_stack[returnSP + 2], "loop var");
        if (type == TYPE_WORD) {
            if (return_stack[returnSP + 4]) {
                emit(VM_STRWORD);
            } else {
                emit(VM_STAWORD);
            }
        } else {
            if (return_stack[returnSP + 4]) {
                emit(VM_STRBYTE);
            } else {
                emit(VM_STABYTE);
            }
        }
        emit(VM_GTE);
        emitldi(return_stack[returnSP + 3], "ret addr");
        emit(VM_BRNCH);
        goto unwind;
    }

    /*
     * Compare loop control variable and limit
     */
    if (val < return_stack[returnSP + 2]) {

        /*
         * If loop not done, increment loop control var, jump back
         * to line after FOR
         */
        if (type == TYPE_WORD) {
            ++(*(int *) (return_stack[returnSP + 1]));
        } else {
            ++(*(unsigned char *) (return_stack[returnSP + 1]));
        }

        backtotop(return_stack[returnSP + 4],
                  (char *) return_stack[returnSP + 3]);

        return RET_SUCCESS;
    }

  unwind:
    /* Done looping, unwind stack */
    pop_return();
    pop_return();
    pop_return();
    pop_return();
    pop_return();
    return RET_SUCCESS;
}

/*************************************************************************/
/* WHILE LOOPS                                                           */
/*************************************************************************/

/*
 * Handles entry into a while loop.
 * startTxtPtr should point to the text of the WHILE statement itself.
 * arg is the evaluated value of the argument to the WHILE.
 */
void dowhile(char *startTxtPtr, unsigned char arg)
{

#ifdef __GNUC__
//print("dowhile()\n");
#endif

    /*
     * Place the following on the return stack when interpreting:
     *   - Magic value WHILEFRAME to indicate WHILE loop stack frame
     *   - Status value as follows:
     *     0: skipFlag was already set so not evaluating my argument
     *     1: skipFlag was clear and I set it (condition false)
     *     2: skipFlag was clear and I left it clear (condition true)
     *   - Line number for the WHILE line (here)
     *   - txtPtr (int*)
     *
     * When compiling:
     *   - Magic value WHILEFRAME
     *   - Runtime PC prior to evaluating WHILE expression
     *   - Runtime PC for patching up the branch
     *   - Dummy value
     *
     */
    push_return(WHILEFRAME);

    if (compile) {
        push_return(rtPCBeforeEval);
        /* **** Value of WHILE expression is on the VM stack **** */
        emit(VM_NOT);
        push_return(rtPC + 1);  /* Address of dummy 0xffff */
        emitldi(0xffff, "TDB");
        emit(VM_BRNCH);
        push_return(0);         /* Dummy */
    } else {
        if (skipFlag) {
            push_return(0);
        } else {
            if (!arg) {
                skipFlag = 1;
                push_return(1);
            } else {
                push_return(2);
            }
        }
        push_return(counter);
        push_return((int) startTxtPtr);
    }
}

/*
 * Handles endwhile statement.
 * Returns RET_SUCCESS on success, RET_ERROR on error
 */
unsigned char doendwhile()
{

#ifdef __GNUC__
//print("doendwhile()\n");
#endif

    if (return_stack[returnSP + 4] != WHILEFRAME) {
        error(ERR_NOWHILE);
        return RET_ERROR;
    }

    if (compile) {
        /*
         * Jump back and re-evaluate the WHILE argument.
         */
        emitldi(return_stack[returnSP + 3], "re-eval");
        emit(VM_JMP);
        /*
         * Fixup the dummy destination address initialized by
         * dowhile() to point to the ENDWHILE.
         */
        emit_fixup(return_stack[returnSP + 2], rtPC);
    } else {
        switch (return_stack[returnSP + 3]) {

        case 0:

            /*
             * If skipFlag was true when we hit the
             * matching WHILE statement, the the value
             * at return_stack[returnSP+3] will be 0.
             */

            goto doret;

        case 1:

            /*
             * If skipFlag was false when we hit the
             * matching WHILE statement, then the value
             * at return_stack[returnSP + 3] will be 1
             * (condition false) or 2 (condition true).
             * If the WHILE was false, then just set
             * clear skipFlag, pop the stack and keep
             * going.  If the WHILE was true, pop the
             * stack and jump back to the WHILE test
             * again.
             */

            skipFlag = 0;
            goto doret;

        case 2:

            /*
             * skipFlag was true when we hit the
             * matching WHILE. Having executed the
             * loop body, now we loop back
             * to the WHILE statement.
             */
            backtotop(return_stack[returnSP + 2],
                      (char *) return_stack[returnSP + 1]);

            goto doret;

        default:
            /* Should never get here! */
            exit(99);
        }
    }

  doret:
    pop_return();
    pop_return();
    pop_return();
    pop_return();

    return RET_SUCCESS;
}

/*
 * Compare two strings up to terminator character.
 * This function takes two char pointers and compares them character
 * by character, up until a terminator character c (or space), which MUST
 * appear in s1.  Returns 0 if equal, 1 if unequal.
 */
unsigned char compareUntil(char *s1, char *s2, char term)
{
    while (*s1 == *s2) {
        if (*s1 == 0) {
            return 1;
        }
        ++s1;
        ++s2;
    }
    /* s2 is allowed to have extra trailing junk */
    if ((*s1 == term) || (*s1 == ' ')) {
        return 0;
    }
    return 1;
}

/*
 * Handle subroutine declaration.
 * This is really only used by the compiler.
 */
unsigned char dosubr()
{
    unsigned char type;
    char name[4];
    unsigned char j;
    unsigned char arraymode;
    var_t *v;
    sub_t *s;

    if (compile) {

        compilingsub = 1;

        print("\nsub ");
        print(readbuf);
        print("\n");

	/*
	 * Create entry in subroutine table
	 * We use heap2 for this because heap1 is used for keeping track
	 * of the location of the variables in the compiled code.
	 * TODO: There is no way to free this without deleting all of heap2 (source code)
	 */
        s = alloc2(sizeof(var_t) + sizeof(int));
    	strncpy(s->name, readbuf, 8);
	s->addr = rtPC;
	s->next = NULL;

        if (subsend) {
            subsend->next = s;
        }
        subsend = s;
        if (!subsbegin) {
            subsbegin = s;
        }
	
        vars_markcallframe();

        /* Update frame pointer */
        emit(VM_SPTOFP);
        rtFP = rtSP;

        if (expect('(')) {
            return RET_ERROR;
        }

        for (;;) {
            eatspace();
            if (*txtPtr == ')') {
                break;
            }
            if (!strncmp(txtPtr, "word ", 5)) {
                type = TYPE_WORD;
            } else if (!strncmp(txtPtr, "byte ", 5)) {
                type = TYPE_BYTE;
            } else {
                error(ERR_ARG);
                return RET_ERROR;
            }
            txtPtr += 5;
            eatspace();
            for (j = 0; j < 4; ++j) {
                name[j] = 0;
            }
            j = 0;
            while (txtPtr && (isalphach(*txtPtr) || isdigitch(*txtPtr))) {
                if (j < 4) {
                    name[j] = *txtPtr;
                }
                ++j;
                ++txtPtr;
            }
            /*
             * If argument is followed by '[]'
             * then switch to pass array by ref
             * mode.
             */
            arraymode = 0;
            if (txtPtr && (*txtPtr == '[')) {
                ++txtPtr;
                if (txtPtr && (*txtPtr == ']')) {
                    ++txtPtr;
                    arraymode = 1;
                } else {
                    error(ERR_ARG);
                    return RET_ERROR;
                }
            }

            /* TODO: Handle pass by reference for arrays */

            /*
             * Set up the variables for the formal parameters,
             * pointing back to the storage already allocated on
             * the eval stack by the caller.  Each time we add
	     * a parameter, adjust the relative addresses of all
	     * the previously handled parameters.
	     */
            v = varslocal;
            while (v) {
                if (v->name[0] != '-') {
                   if (type == TYPE_WORD) {
                       *(int *) ((unsigned char *) v + sizeof(var_t)) += 2;
                   } else {
                       *(int *) ((unsigned char *) v + sizeof(var_t)) += 1;
                   }
		}
                v = v->next;
            }

            v = alloc1(sizeof(var_t) + sizeof(int));
            *(int *) ((unsigned char *) v + sizeof(var_t)) = 4; // Skip over return address and frame pointer
            strncpy(v->name, name, 4);
            v->type = ((arraymode & 0x0f) << 4) | type;
            v->next = NULL;

            if (varsend) {
                varsend->next = v;
            }
            varsend = v;
            if (!varsbegin) {
                varsbegin = v;
                varslocal = v;
            }

            eatspace();
            if (*txtPtr == ',') {
                ++txtPtr;       /* Eat the comma */
            }
        }
        if (expect(')')) {
            return RET_ERROR;
        }

    } else {
        /* Error if we just run into this line! */
        error(ERR_RUNSUB);
        return RET_ERROR;
    }
    return RET_SUCCESS;
}

/*
 * Handle endsub
 */
unsigned char doendsubr()
{
    if (compile) {
        rtSP = rtFP;
        compilingsub = 0;
        vars_deletecallframe();
    }
    doreturn(0); // THINK ABOUT THIS
    return RET_SUCCESS;
}

/*
 * Perform call instruction
 * Expects sub name to call in readbuf
 * Return RET_SUCCESS if successful, RET_ERROR on error
 */
unsigned char docall()
{
    unsigned char type;
    char *p;
    int arg;
    char name[4];
    char name2[4];
    unsigned char j;
    unsigned char arraymode;
    var_t *oldvarslocal;
    var_t *newvarslocal;
    var_t *array;
    sub_t *s;
    unsigned char argbytes = 0;
    struct lineofcode *l = program;
    int origcounter = counter;
    unsigned char local = 0;

    /*
     * Do this before evaluating arguments, which overwrites readbuf
     */
    if (compile) {
        s = alloc2(sizeof(var_t) + sizeof(int));
        strncpy(s->name, readbuf, 8);
    }

    counter = -1;
    while (l) {
        p = l->line;
        ++counter;

        skipFlag = 0;

        while (p && (*p == ' ')) {
            ++p;
        }
        if (!strncmp(p, "sub ", 4)) {
            p += 4;
            while (p && (*p == ' ')) {
                ++p;
            }

            if (!compareUntil(p, readbuf, '(')) {

                /*
                 * Here we are parsing two lines at a time:
                 * The call (at *txtPtr as usual) and the
                 * sub being called (at *p).
                 */

                /* Eat the subroutine name at *p */
                while (p && (*p != '(')) {
                    ++p;
                }
                if (!p) {
                    error(ERR_EXPECT);
                    printchar('(');
                    return RET_ERROR;
                }
                ++p;            /* Eat the '(' */

                /*
                 * Set up txtPtr to start passing the argument
                 * list of the call
                 */
                eatspace();
                if (expect('(')) {
                    return RET_ERROR;
                }

                if (!compile) {
                    /*
                     * Will need this later for looking up
                     * things in the 'old' frame
                     */
                    oldvarslocal = varslocal;

                    /*
                     * For CALL, stack frame is:
                     *  - CALLFRAME magic number
                     *  - line number of CALL
                     *  - Pointer to just after the call statement
                     *    (set further down in the code)
                     */
                    push_return(CALLFRAME);
                    push_return(origcounter);

                    vars_markcallframe();

                    newvarslocal = varslocal;
                }

                /*
                 * Iterate through the formal parameter
                 * list of the sub (at *p).
                 *
                 * For word and byte scalar parameters, we
                 * instantiate a local of the appropriate
                 * type, evaluate the corresponding expression
                 * in the call and store the result in this
                 * new local.
                 *
                 * For arrays we copy the header, leaving
                 * the pointer to the original global data
                 * intact.  Effectively, this gives arrays
                 * pass by reference semantics (similar to C).
                 * This trick works when passing literal
                 * arrays only.
                 */
                for (;;) {
                    while (p && (*p == ' ')) {
                        ++p;
                    }
                    if (!p) {
                        error(ERR_ARG);
                        return RET_ERROR;
                    }
                    if (*p == ')') {
                        break;
                    }
                    if (!strncmp(p, "word ", 5)) {
                        type = TYPE_WORD;
                    } else if (!strncmp(p, "byte ", 5)) {
                        type = TYPE_BYTE;
                    } else {
                        error(ERR_ARG);
                        return RET_ERROR;
                    }
                    p += 5;
                    while (p && (*p == ' ')) {
                        ++p;
                    }
                    if (!p) {
                        error(ERR_ARG);
                        return RET_ERROR;
                    }
                    for (j = 0; j < 4; ++j) {
                        name[j] = 0;
                    }
                    j = 0;
                    while (p && (isalphach(*p) || isdigitch(*p))) {
                        if (j < 4) {
                            name[j] = *p;
                        }
                        ++j;
                        ++p;
                    }
                    /*
                     * If argument is followed by '[]'
                     * then switch to pass array by ref
                     * mode.
                     */
                    arraymode = 0;
                    if (p && (*p == '[')) {
                        ++p;
                        if (p && (*p == ']')) {
                            ++p;
                            arraymode = 1;
                        } else {
                            error(ERR_ARG);
                            return RET_ERROR;
                        }
                    }

                    /*
                     * Now we go back to looking at the 
                     * call arguments
                     */

                    /* If end of line, error */
                    if (!(*txtPtr)) {
                        error(ERR_ARG);
                        return RET_ERROR;
                    }
                    if (*txtPtr == ')') {
                        error(ERR_ARG);
                        return RET_ERROR;
                    }
                    if (!arraymode) {
                        /*
                         * Pass scalar value
                         */

                        if (!compile) {
                            /* Back to old frame for lookup */
                            varslocal = oldvarslocal;
                        }
                        if (eval(0, &arg)) {
                            /* No expression found */
                            varslocal = newvarslocal;
                            error(ERR_ARG);
                            return RET_ERROR;
                        }
                        if (compile) {
                            if (type == TYPE_WORD) {
                                emit(VM_PSHWORD);
				argbytes += 2;
                            } else {
                                emit(VM_PSHBYTE);
				++argbytes;
                            }
                        } else {
                            /* Back to new frame to create var */
                            varslocal = newvarslocal;
                            createintvar(name, type, 0, 1, arg, 0);
                        }
                    } else {
                        /*
                         * Array pass-by-reference
                         */
                        for (j = 0; j < 4; ++j) {
                            name2[j] = 0;
                        }
                        j = 0;
                        while (txtPtr && (isalphach(*txtPtr)
                                          || isdigitch(*txtPtr))) {
                            if (j < 4) {
                                name2[j] = *txtPtr;
                            }
                            ++txtPtr;
                            ++j;
                        }
                        if (!compile) {
                            /* Back to old frame for lookup */
                            varslocal = oldvarslocal;
                        }
                        array = findintvar(name2, &local);
                        if (!array) {
                            error(ERR_VAR);
                            return RET_ERROR;
                        }
                        /* j holds number of dimensions */
                        j = (array->type & 0xf0) >> 4;
                        if (((array->type & 0x0f) != type) || (j == 0)) {
                            error(ERR_TYPE);
                            return RET_ERROR;
                        }
                        if (compile) {
                            /* TODO: Handle pass by reference for arrays for compiled code */
                        } else {
                            /* Back to new frame to create var */
                            varslocal = newvarslocal;
                            createintvar(name,
                                         type,
                                         j,
                                         *(getptrtoscalarword(array) + 1),
                                         0, *getptrtoscalarword(array));
                        }
                    }
                    eatspace();
                    if (*txtPtr == ',') {
                        ++txtPtr;
                    }
                    eatspace();

                    while (p && (*p == ' ')) {
                        ++p;
                    }

                    if (!p) {
                        error(ERR_ARG);
                        return RET_ERROR;
                    }

                    if (*p == ',') {
                        ++p;    /* Eat the comma */
                    }
                }

                eatspace();
                if (expect(')')) {
                    return RET_ERROR;
                }

                if (compile) {
                    emitldi(0xffff, readbuf);
                    /*
                     * Create entry in call table
                     * We use heap2 for this because heap1 is used for keeping track
                     * of the location of the variables in the compiled code.
                     * TODO: There is no way to free this without deleting all of heap2 (source code)
                     */
                    s->addr = rtPC - 2;
                    s->next = NULL;

                    if (callsend) {
                        callsend->next = s;
                    }
                    callsend = s;
                    if (!callsbegin) {
                        callsbegin = s;
                    }

                    emit(VM_JSR);

                    /* Caller must drop the arguments
                     * pushed to call stack above */
                    for (j = 0; j < argbytes; ++j) {
                        emit(VM_POPBYTE);
                        emit(VM_DROP);
                    }
                } else {
                    /* Stash pointer to just after the call stmt */
                    push_return((int) txtPtr);

                    /*
                     * Set up parser to start executing first
                     * line of subroutine
                     */
                    current = l->next;
                    ++counter;
                    txtPtr = current->line;
                }
                return RET_SUCCESS;
            }
        }
        l = l->next;
    }
    error(ERR_NOSUB);
    counter = origcounter;
    return RET_ERROR;
}

/*
 * Handle return from subroutine.
 * Parameter retvalue is the value to be returned to the caller.
 * Returns RET_SUCCESS on success, RET_ERROR on error
 */
unsigned char doreturn(int retvalue)
{
    if (compile) {

        /*
	 * Return value is already on evaluation stack
	 */

        /* Update stack pointer to drop local variables */
        emit(VM_FPTOSP);

        /* And done! */
        emit(VM_RTS);

    } else {

        /*
         * Search the stack to find the first CALLFRAME.  This allows us
         * to unwind any inner stackframes (for example where we return
         * from within a FOR loop or IF statement.)
         */

        int p = returnSP + 1;

        while (p <= RETSTACKSZ - 1) {
            if (return_stack[p] == CALLFRAME) {
                /*
                 * Unwind the stack.
                 */
                returnSP = p;
                goto found;
            }
            ++p;
        }

        error(ERR_STACK);
        return RET_ERROR;

      found:
        /* Stash the return value */
        retregister = retvalue;

        vars_deletecallframe();

        backtotop(return_stack[p - 1], (char *) return_stack[p - 2]);
    }
    return RET_SUCCESS;
}

/*************************************************************************/

/*
 * Parse a decimal integer constant.
 * The text to parse is pointed to by txtPtr.
 * The result is placed in val.
 * If successful returns 0, otherwise 1.
 */
unsigned char parseint(int *val)
{
    *val = 0;
    if (!(*txtPtr)) {
        return 1;
    }
    if (!isdigitch(*txtPtr)) {
        return 1;
    }
    do {
        *val *= 10;
        *val += *txtPtr - '0';
        ++txtPtr;
    } while (isdigitch(*txtPtr));
    return 0;
}

/*
 * Return value of hex char
 */
unsigned char hexchar2val(char c)
{
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return c - '0';
}

/*
 * Parse a hexadecimal integer constant.
 * The text to parse is pointed to by txtPtr.
 * The result is placed in val.
 * If successful returns 0, otherwise 1.
 */
unsigned char parsehexint(int *val)
{
    *val = 0;
    if (!(isdigitch(*txtPtr) || ((*txtPtr >= 'a') && (*txtPtr <= 'f')))) {
        return 1;
    }
    do {
        *val *= 16;
        *val += hexchar2val(*txtPtr);
        ++txtPtr;
    } while (isdigitch(*txtPtr) || ((*txtPtr >= 'a') && (*txtPtr <= 'f')));
    return 0;
}

/*
 * Statement tokens - must be in order with no gaps in sequence
 */
#define TOK_COMM     150        /* (comment) must be lowest-numbered token */
#define TOK_PRDEC    151        /* pr.dec        */
#define TOK_PRDEC_S  152        /* pr.dec.s      */
#define TOK_PRHEX    153        /* pr.hex        */
#define TOK_PRMSG    154        /* pr.msg        */
#define TOK_PRNL     155        /* pr.nl         */
#define TOK_PRSTR    156        /* pr.str        */
#define TOK_PRCH     157        /* pr.ch         */
#define TOK_KBDCH    158        /* kbd.ch        */
#define TOK_KBDLN    159        /* kbd.ln        */
#define TOK_QUIT     160        /* quit          */
#define TOK_CLEAR    161        /* clear         */
#define TOK_VARS     162        /* vars          */
#define TOK_WORD     163        /* word          */
#define TOK_BYTE     164        /* byte          */
#define TOK_RUN      165        /* run           */
#define TOK_COMPILE  166        /* comp          */
#define TOK_NEW      167        /* new           */
#define TOK_SUBR     168        /* sub           */
#define TOK_ENDSUBR  169        /* endsub        */
#define TOK_IF       170        /* if            */
#define TOK_ELSE     171        /* else          */
#define TOK_ENDIF    172        /* endif         */
#define TOK_FREE     173        /* free          */
#define TOK_CALL     174        /* call sub      */
#define TOK_RET      175        /* return        */
#define TOK_FOR      176        /* for           */
#define TOK_ENDFOR   177        /* endfor        */
#define TOK_WHILE    178        /* while         */
#define TOK_ENDW     179        /* endwhile      */
#define TOK_END      180        /* end           */

/*
 * All the following tokens do not require trailing whitespace
 * Careful - the ordering matters!
 */
#define TOK_POKEWORD 181        /* poke word (*) */
#define TOK_POKEBYTE 182        /* poke byte (^) */

/* Line editor commands */
#define TOK_LOAD    183         /* Editor: load        */
#define TOK_SAVE    184         /* Editor: save        */
#define TOK_LIST    185         /* Editor: list        */
#define TOK_CHANGE  186         /* Editor: modify line */
#define TOK_APP     187         /* Editor: append line */
#define TOK_INS     188         /* Editor: insert line */
#define TOK_DEL     189         /* Editor: delete line */

/*
 * Used for the stmnttabent type field.  Code in parseline() uses this
 * value to determine how to handle parameters for the statement.
 *
 *  FULLLINE: full-line statement where the entire line 'belongs' to the
 *            statement (comments, sub and lbl are like this.)
 *  NOARGS: no arguments permitted.
 *  ONEARG: one expression is expected and evaluated.  No further arguments
 *          permitted.
 *  TWOARGS: two expressions are expected, separated by a comma
 *  INITIALARG: one expression is evaluated.  Any subsequent arguments may be
 *              evaluated by custom code for each statement.
 *  ONESTRARG: a string constant in quotes is expected
 *  INITIALNAMEARG: a single name is evaluated.  Any subsequent arguments may
 *                  be evaluated by custom code for each statement.  The name
 *                  must start with alpha character and has no spaces.
 *  CUSTOM: the statement has its own custom code to handle parameters
 */
enum stmnttype { FULLLINE,
    NOARGS,
    ONEARG,
    TWOARGS,
    INITIALARG,
    ONESTRARG,
    INITIALNAMEARG,
    CUSTOM
};

/*
 * Represents an entry in the statement table
 */
struct stmnttabent {
    char *name;
    unsigned char token;
    enum stmnttype type;
};

/*
 * Statement table
 * Must be in order of sequentially increasing token value, so that
 * stmnttabent[t - TOK_COMM] is the line matching token t
 *
 * Also note that if a one name is a prefix of another, the longer one
 * must be first (so println goes before print).
 */
struct stmnttabent stmnttab[] = {

    /* Statements */
    {"\'", TOK_COMM, FULLLINE},
    {"pr.dec", TOK_PRDEC, ONEARG},
    {"pr.dec.s", TOK_PRDEC_S, ONEARG},
    {"pr.hex", TOK_PRHEX, ONEARG},
    {"pr.msg", TOK_PRMSG, ONESTRARG},
    {"pr.nl", TOK_PRNL, NOARGS},
    {"pr.str", TOK_PRSTR, ONEARG},
    {"pr.ch", TOK_PRCH, ONEARG},
    {"kbd.ch", TOK_KBDCH, ONEARG},
    {"kbd.ln", TOK_KBDLN, TWOARGS},
    {"quit", TOK_QUIT, NOARGS},
    {"clear", TOK_CLEAR, NOARGS},
    {"vars", TOK_VARS, NOARGS},
    {"word", TOK_WORD, CUSTOM},
    {"byte", TOK_BYTE, CUSTOM},
    {"run", TOK_RUN, NOARGS},
    {"comp", TOK_COMPILE, NOARGS},
    {"new", TOK_NEW, NOARGS},
    {"sub", TOK_SUBR, INITIALNAMEARG},
    {"endsub", TOK_ENDSUBR, NOARGS},
    {"if", TOK_IF, ONEARG},
    {"else", TOK_ELSE, NOARGS},
    {"endif", TOK_ENDIF, NOARGS},
    {"free", TOK_FREE, NOARGS},
    {"call", TOK_CALL, INITIALNAMEARG},
    {"return", TOK_RET, ONEARG},
    {"for", TOK_FOR, CUSTOM},
    {"endfor", TOK_ENDFOR, NOARGS},
    {"while", TOK_WHILE, ONEARG},
    {"endwhile", TOK_ENDW, NOARGS},
    {"end", TOK_END, NOARGS},
    {"*", TOK_POKEWORD, INITIALARG},
    {"^", TOK_POKEBYTE, INITIALARG},

    /* Editor commands */
    {":r", TOK_LOAD, ONESTRARG},
    {":w", TOK_SAVE, ONESTRARG},
    {":l", TOK_LIST, CUSTOM},
    {":c", TOK_CHANGE, INITIALARG},
    {":a", TOK_APP, ONEARG},
    {":i", TOK_INS, ONEARG},
    {":d", TOK_DEL, INITIALARG}
};

/*
 * Attempt to find statement keyword
 * Returns the token or ILLEGAL
 *
 * Uses table stmnttab, returning the token corresponding to the first
 * matching name.  Also checks for either a space or end of line following
 * the keyword, or will not declare a match.
 */
unsigned char matchstatement()
{
    unsigned char i;
    unsigned char len;
    char c;
    struct stmnttabent *s;

    /* Be careful to update this number (40) when adding new tokens */
    for (i = 0; i < 40; ++i) {
        s = &(stmnttab[i]);
        len = strlen(s->name);
        if (!strncmp(txtPtr, s->name, len)) {
            /*
             * Check for whitespace on 151 <= tokens <= 178 only
             * Careful - be sure to update this when adding
             * new tokens!
             */
            if ((s->token > 178) || (s->token < 151)) {
                return s->token;
            }
            c = *(txtPtr + len);
            if ((c == 0) || (c == ' ') || (c == ';')) {
                return s->token;
            }
        }
    }
    return ILLEGAL;
}

/*
 * Used to check no arguments are passed to statements that do not take them
 * Returns 0 if end of line or semicolon next, 1 otherwise.
 */
unsigned char checkNoMoreArgs()
{
    eatspace();
    if (*txtPtr && (*txtPtr != ';')) {
        error(ERR_EXTRA);
        printchar(' ');
        print(txtPtr);
        return 1;
    }
    return 0;
}

/* Parse a line from the input buffer
 * Handles statements
 * Starts reading from location of txtPtr
 * Returns:
 *   0: Keep executing
 *   1: Normal stop
 *   2: Error stop
 *   3: User stop / escape
 */
unsigned char parseline()
{
    int token;
    int arg;
    int arg2;
    char *p;
    char *startTxtPtr;
    struct stmnttabent *s;

    for (;;) {

        /* See if user requested stop */
        if (checkInterrupted()) {
            return 3;
        }

        eatspace();

        while (*txtPtr == ';') {
            ++txtPtr;
            if (!(*txtPtr)) {
                return 0;
            }
            eatspace();
        }

        if (!(*txtPtr)) {
            return 0;
        }

        startTxtPtr = txtPtr;

        token = matchstatement();

        /*
         * If skipFlag is set, then only process those tokens that
         * manipulate skipFlag:
         *   'if / else / endif'
         *   'while / endw'
         * Skip all others.
         */
        if (skipFlag) {
            if ((token != TOK_IF) &&
                (token != TOK_ELSE) &&
                (token != TOK_ENDIF) &&
                (token != TOK_WHILE) && (token != TOK_ENDW)) {

                /*
                 * Eat the statement up to semicolon or the
                 * end.
                 */
                while (*txtPtr && (*txtPtr != ';')) {
                    ++txtPtr;
                }
                continue;
            }
        }

        if (token == ILLEGAL) {

#ifdef CBM
            /*
             * If the first character is a digit then treat
             * this as an editor 'change line' command.  This
             * allows the VIC-20/C64 screen editor to work the
             * same way as in BASIC.
             */

            if (isdigitch(*txtPtr)) {

                token = TOK_CHANGE;
                s = &(stmnttab[token - TOK_COMM]);

            } else {
#endif
                /*
                 * Variable assignment winds up here
                 */
                if (assignorcreate(LET_MODE)) {
                    return 2;   /* Error */
                }
                continue;
#ifdef CBM
            }
#endif

        } else {

            s = &(stmnttab[token - TOK_COMM]);

            /* Eat the keyword */
            txtPtr += strlen(s->name);

            eatspace();
        }

        /*
         * If we are compiling it is good to keep a copy of the
         * VM program counter just before we begin argument 
         * handling.  This is useful for re-evaluating WHILE loop
         * guards, for example!
         */
        rtPCBeforeEval = rtPC;

        /*
         * Generic parameter handling based on statement type.
         */
        switch (s->type) {

        case NOARGS:
            /* Check end of input */
            arg = checkNoMoreArgs();
            if (arg) {
                return 2;
            }
            break;
        case ONEARG:
            /* Evaluate one arg and check end of input */
            if (eval(1, &arg)) {
                return 2;
            }
            break;
        case TWOARGS:
            /* Evaluate one arg don't check end of input */
            if (eval(0, &arg)) {
                return 2;
            }
            eatspace();
            if (expect(',')) {
                return 2;
            }
            /* Evaluate second arg, don't check end of input */
            if (eval(0, &arg2)) {
                return 2;
            }
            break;
        case INITIALARG:
            /* Evaluate one arg, don't check end of input */
            if (eval(0, &arg)) {
                return 2;
            }
            break;
        case ONESTRARG:
            /* Parse quoted string, place it in readbuf */
            if (!(*txtPtr == '"')) {
                return 2;
            }
            ++txtPtr;
            p = readbuf;
            while (*txtPtr && (*txtPtr != '"')) {
                *(p++) = *(txtPtr++);
            }
            *p = '\0';
            if (*txtPtr == '"') {
                ++txtPtr;
            } else {
                error(ERR_STR);
                return 2;
            }
            arg = checkNoMoreArgs();
            if (arg) {
                return 2;
            }
            break;
        case INITIALNAMEARG:
            /* Evaluate name, place in readbuf */
            /* Don't check end of input */
            if (!isalpha(*txtPtr)) {
                return 2;
            }
            p = readbuf;
            while (*txtPtr && (isalphach(*txtPtr) || isdigitch(*txtPtr))) {
                *(p++) = *(txtPtr++);
            }
            *p = '\0';
            break;
        case FULLLINE:
            /* Eat the line */
            while (*txtPtr) {
                ++txtPtr;
            }
            break;
#ifdef __GNUC__
        case CUSTOM:
            break;
#endif
        }

        /*
         * Code for individual statements.
         */
        switch (token) {
        case TOK_COMM:
            break;
        case TOK_QUIT:
#ifdef C64
            /* Restore normal NMI vector on C64 */
            POKE(808, 237);
#elif defined(VIC20)
            /* Restore normal NMI vector on VIC20 */
            POKE(808, 112);
#endif

            print("Bye!\n");
            EXIT(0);
        case TOK_PRDEC:
            if (compile) {
                emit(VM_PRDEC);
            } else {
                printdec(arg);
            }
            break;
        case TOK_PRDEC_S:
            if (compile) {
                emit(VM_DUP);   /* Preserve arg on the stack */
                emitldi(0x8000, 0);
                emit(VM_BITAND);
                emit(VM_NOT);
                emitldi(rtPC + 9, 0);   /* Jump over printing of '-' */
                emit(VM_BRNCH);
                emitldi('-', "minus");
                emit(VM_PRCH);
		emit(VM_NEG);
                emit(VM_PRDEC);
            }
            if (arg < 0) {
                printchar('-');
                arg = -arg;
            }
            printdec(arg);
            break;
        case TOK_PRHEX:
            if (compile) {
                emit(VM_PRHEX);
            } else {
                printhex(arg);
            }
            break;
        case TOK_PRMSG:
	    if (compile) {
		emitprmsg();
	    } else {
            	print(readbuf);
	    }
            break;
        case TOK_PRNL:
            if (compile) {
                emitldi(10, "nl");
                emit(VM_PRCH);
            } else {
                printchar('\n');
            }
            break;
        case TOK_PRSTR:
	    if (compile) {
		emit(VM_PRSTR);
	    } else {
                print((char *) arg);
	    }
            break;
        case TOK_PRCH:
            if (compile) {
                emit(VM_PRCH);
            } else {
                printchar(arg);
            }
            break;
        case TOK_KBDCH:
	    if (compile) {
		/* Address should be on the eval stack already */
	        emit(VM_KBDCH);
		/* Now the keycode is pushed to the eval stack also */
		emit(VM_SWAP);
		emit(VM_STABYTE);
	    } else {
#ifdef A2E
                /* Loop until we get a keypress */
                while (!(arg2 = getkey()));
                *(char *) arg = arg2;
#elif defined(CBM)
                /* Loop until we get a keypress */
                while (!(*(char *) arg = cbm_k_getin()));
#else
                print("kbd.ch unimplemented on Linux\n");
#endif
	    }
            break;
        case TOK_KBDLN:
	    if (compile) {
                /* Address and length should both be on the eval stack */
		emit(VM_KBDLN);
            } else {
                getln((char *) arg, arg2);
	    }
            break;
        case TOK_CLEAR:
            clearvars();
            break;
        case TOK_VARS:
            printvars();
            break;
        case TOK_WORD:
            if (assignorcreate(WORD_MODE)) {
                return 2;
            }
            break;
        case TOK_BYTE:
            if (assignorcreate(BYTE_MODE)) {
                return 2;
            }
            break;
        case TOK_RUN:
            run(0);             /* Start from beginning */
            break;
        case TOK_COMPILE:
            compile = 1;
	    subsbegin = subsend = NULL;
	    callsbegin = callsend = NULL;
            CLEARRTCALLSTACK();
            run(0);
            emit(VM_END);
	    linksubs();
            writebytecode();
            compile = 0;
            break;
        case TOK_NEW:
            new();
            break;
        case TOK_SUBR:
            if (dosubr()) {
                return 2;
            }
            break;
	case TOK_ENDSUBR:
	    if (doendsubr()) {
                return 2;
	    }
	    break;
        case TOK_CALL:
            if (docall()) {
                return 2;
            }
	    if (compile) {
	        /* Drop the return value */
	        emit(VM_DROP);
	    } else {
                /* If we were called from immediate mode ... */
                /* Switch to run mode and continue */
                if (return_stack[returnSP + 2] == -1) {
                    run(1);
                }
	    }
            break;
        case TOK_RET:
            if (doreturn(arg)) {
                /* Error */
                return 2;
            }

            /*
             * If this was a function invocation, just
             * return and let P() continue with its job!
             */
            if (return_stack[returnSP + 2] == -2) {
                return 1;
            }

            break;
        case TOK_IF:
            doif(arg);
            break;
        case TOK_ELSE:
            if (doelse()) {
                return 2;
            }
            break;
        case TOK_ENDIF:
            if (doendif()) {
                return 2;
            }
            break;
        case TOK_FOR:
            if (assignorcreate(FOR_MODE)) {
                return 2;
            }
            break;
        case TOK_ENDFOR:
            if (doendfor()) {
                return 2;
            }
            break;
        case TOK_WHILE:
            dowhile(startTxtPtr, arg);
            break;
        case TOK_ENDW:
            if (doendwhile()) {
                return 2;
            }
            break;
        case TOK_END:
            if (compile) {
                emit(VM_END);
            } else {
                return 1;       /* Normal stop */
            }
            break;
        case TOK_FREE:
#ifdef CC65
            printdec(getfreespace1());
            print(" vars, ");
            printdec(getfreespace2());
            print(" code\n");
#else
            printdec(getfreespace1());
            print(" vars, ");
            print("code space pretty much unlimited!\n");
#endif
            break;
        case TOK_POKEWORD:
            eatspace();
            if (expect('=')) {
                return 2;
            }
            if (eval(1, &arg2)) {
                return 2;
            }
            if (compile) {
                emit(VM_SWAP);
                emit(VM_STAWORD);
                return 0;
            }
            *(int *) arg = arg2;
            break;
        case TOK_POKEBYTE:
            eatspace();
            if (expect('=')) {
                return 2;
            }
            if (eval(1, &arg2)) {
                return 2;
            }
            if (compile) {
                emit(VM_SWAP);
                emit(VM_STABYTE);
                return 0;
            }
            *(unsigned char *) arg = arg2;
            break;
        case TOK_APP:
            findline(arg);
            if (!current) {
                error(ERR_LINE);
                break;
            }
            editmode = 1;
            break;
        case TOK_INS:
            if (arg <= 1) {
                editmode = 2;   /* Special mode for insert
                                   first line */
            } else {
                findline(arg - 1);
                if (!current) {
                    error(ERR_LINE);
                    break;
                }
                editmode = 1;
            }
            break;
        case TOK_DEL:
            eatspace();
            if (!(*txtPtr)) {
                deleteline(arg, arg);   /* One arg */
                break;
            }
            if (expect(',')) {
                return 2;
            }
            if (eval(1, &arg2)) {
                return 2;
            }
            deleteline(arg, arg2);      /* Two args */
            break;
        case TOK_CHANGE:
            eatspace();
            if (expect(':')) {
                return 2;
            }
            findline(arg);
            if (!current) {
                error(ERR_LINE);
                break;
            }
            changeline(txtPtr);
            /* Don't execute the changed code yet! */
            return 0;
            break;
        case TOK_LIST:
            if (!(*txtPtr)) {
                list(1, 32767); /* No args */
                break;
            }
            if (eval(0, &arg)) {
                return 2;
            }
            eatspace();
            if (!(*txtPtr)) {
                list(arg, 32767);       /* One arg */
                break;
            }
            if (expect(',')) {
                return 2;
            }
            if (eval(1, &arg2)) {
                return 2;
            }
            list(arg, arg2);    /* Two args */
            break;
        case TOK_LOAD:
            if (readfile()) {
                return 2;       /* Error */
            }
            /* Because readfile() trashes lnbuf ... */
            return 0;
            break;
        case TOK_SAVE:
            if (writefile()) {
                return 2;       /* Error */
            }
            break;
        default:
            /* Should never get here */
            EXIT(99);
        }
    }
    return 0;
}

/*
 * Expects filename in readbuf.
 * If writemode = 0 then it opens file for reading, otherwise for writing.
 * Returns 0 if OK, 1 if error.
 */
unsigned char openfile(unsigned char writemode)
{
    char *readPtr = readbuf;

    if (writemode) {
        print("Writing ");
    } else {
        print("Reading ");
    }
    print(readPtr);
    printchar(':');
    while (*readPtr) {
        ++readPtr;
    }
#ifdef CBM
    /* Commodore only, append ',s' for SEQ file */
    *(readPtr++) = ',';
    *(readPtr++) = 's';
#endif
    *readPtr = '\0';

    readPtr = readbuf;

#ifdef CBM
    /* Commodore */
    if (cbm_open(1, 8, (writemode ? CBM_WRITE : CBM_READ), readPtr)) {
        error(ERR_FILE);
        return 1;
    }
#else

#ifdef A2E
    _filetype = 4;              /* Text file */
#endif

    /* POSIX */
    fd = fopen(readPtr, (writemode ? "w" : "r"));
    if (fd == NULL) {
        error(ERR_FILE);
        return 1;
    }
#endif

    return 0;

}

/*
 * Load program from file.
 * Expects filename in readbuf.
 * Returns 0 if OK, 1 if error.
 * NOTE: Trashes lnbuf !!
 */
unsigned char readfile()
{
    unsigned char i;
    unsigned char j;
    unsigned int bytes;
    unsigned char foundEOL;
    unsigned char bytesInBuf = 0;
    char *readPtr = readbuf;
    int donereading = 0;
    int count = 0;

    if (openfile(0)) {
        return 1;
    }

    clearvars();
    new();

    readPtr = readbuf;
    do {
        if (!donereading) {
#ifdef DEBUG_READFILE
            print("About to read ");
            printdec(255 - bytesInBuf);
            print(" bytes\n");
#endif

#ifdef CBM
            /* Commodore */
            bytes = cbm_read(1, readPtr, 255 - bytesInBuf);
#else
            /* POSIX */
            bytes = fread(readPtr, 1, 255 - bytesInBuf, fd);

#endif
            if (bytes == -1U) {
                error(ERR_FILE);
#ifdef CBM
                /* Commodore */
                cbm_close(1);
#else
                /* POSIX */
                fclose(fd);
#endif
                return 1;
            }
            if (!bytes) {
                donereading = 1;
            }

            readPtr += bytes;
            bytesInBuf += bytes;

#ifdef DEBUG_READFILE
            print("Read ");
            printdec(bytes);
            print(" bytes\nBuf[");
            for (i = 0; i < bytesInBuf; ++i) {
                printchar(readbuf[i]);
            }
            print("]\n");
#endif
        }

        foundEOL = 0;
        for (i = 0; i < bytesInBuf; ++i) {
            if (readbuf[i] == 10 || readbuf[i] == 13) {
                strncpy(lnbuf, readbuf, i);
                lnbuf[i] = 0;
                for (j = i + 1; j < bytesInBuf; ++j) {
                    readbuf[j - i - 1] = readbuf[j];
                }
                readPtr = readbuf + bytesInBuf - i - 1;
                bytesInBuf -= (i + 1);
                foundEOL = 1;
                break;
            }
        }

        if (foundEOL == 1) {
            txtPtr = lnbuf;
            if (!count) {
                insertfirstline(txtPtr);
                findline(1);
            } else {
                appendline(txtPtr);
            }
            ++count;
        } else {
            if (bytesInBuf == 255) {
                error(ERR_FILE);
#ifdef CBM
                /* Commodore */
                cbm_close(1);
#else
                /* Apple II and POSIX */
                fclose(fd);
#endif
                return 1;
            }
            /* Handle last line with missing CRLF */
            if (donereading == 1 && bytesInBuf) {
                readbuf[bytesInBuf] = '\0';
                appendline(readbuf);
                ++count;
                break;
            }
        }

    } while (bytes || bytesInBuf);

#ifdef CBM
    /* Commodore */
    cbm_close(1);
#else
    /* Apple II and POSIX */
    fclose(fd);
#endif

    printdec(count);
    print(" lines\n");
    return 0;
}

/*
 * Save program to file.
 * Expects filename in readbuf.
 * Returns 0 if OK, 1 if error.
 */
unsigned char writefile()
{
    unsigned int bytes;
    unsigned int index;

    if (openfile(1)) {
        return 1;
    }

    current = program;
    while (current) {
        index = 0;
        for (index = 0; index < strlen(current->line); ++index) {
#ifdef CBM
            /* Commodore */
            bytes = cbm_write(1, current->line + index, 1);
#else
            /* POSIX */
            bytes = fwrite(current->line + index, 1, 1, fd);
#endif
            if (!bytes) {
                goto error;
            }
        }
#ifdef CBM
        /* Commodore */
        bytes += cbm_write(1, "\n", 1);
#elif defined(A2E)
        /* Apple II */
        bytes += fwrite("\r", 1, 1, fd);
#else
        /* POSIX */
        bytes += fwrite("\n", 1, 1, fd);
#endif
        if (!bytes) {
            goto error;
        }
        current = current->next;
    }

#ifdef CBM
    /* Commodore */
    cbm_close(1);
#else
    /* POSIX */
    fclose(fd);
#endif
    print("OK\n");
    return 0;

  error:
#ifdef CBM
    /* Commodore */
    cbm_close(1);
#else
    /* POSIX */
    fclose(fd);
#endif
    error(ERR_FILE);
    return 1;
}

void run(unsigned char cont)
{
    int status = 0;

    calllevel = 0;
    skipFlag = 0;
    if (cont == 0) {
        counter = 0;
        clearvars();
        returnSP = RETSTACKSZ - 1;
        current = program;
    }
    while (current && !status) {
        txtPtr = current->line;
        status = parseline();
        /* parseline() can set current to NULL when return is to
         * immediate mode */
        if (!current) {
            break;
        }
        current = current->next;
        ++counter;
    }
    switch (status) {
    case 2:
        print(" err at ");
        printdec(counter);
        printchar('\n');
        returnSP = (RETSTACKSZ - 1);
        skipFlag = 0;
        break;
    case 3:
        print("\nBrk at ");
        printdec(counter);
        printchar('\n');
        returnSP = (RETSTACKSZ - 1);
        skipFlag = 0;
        break;
    }
}

/*
 * Perform linkage.
 * The subroutine definitions are in the list that starts with subsbegin.
 * The subroutine calls are in the list that starts with callsbegin.
 */
void linksubs() {
	sub_t *call;
	sub_t *sub;
	call = callsbegin;
	while (call) {
		sub = subsbegin;
		
		while (strncmp(sub->name, call->name, 8)) {
			sub = sub->next;
			if (!sub) {
				error(ERR_LINK);
				return;
			}
		}
		emit_fixup(call->addr, sub->addr);
		call = call->next;
	}
}

void list(unsigned int startline, unsigned int endline)
{
    unsigned int count = 1;

    current = program;
    while (current) {
        if ((count >= startline) && (count <= endline)) {
#ifdef CBM
            printchar(28);      /* Red */
            printchar(18);      /* Reverse On */
#elif defined(A2E)
            revers(1);
#endif
            printdec(count);
#ifdef CBM
            printchar(':');     /* To make scrn editor work */
            printchar(144);     /* Black */
            printchar(146);     /* Reverse Off */
#elif defined(A2E)
            revers(0);
#endif
            print(current->line);
            printchar('\n');
        }
        ++count;
        current = current->next;
    }
}

/*
 * Clear the operator and operand stacks prior to evaluating expression.
 */
#define clearexprstacks() \
	operandSP = STACKSZ - 1; \
	operatorSP = STACKSZ - 1; \
	push_operator_stack(SENTINEL);


/*
 * Entry point.
 */
#ifdef __GNUC__
int
#else
void
#endif
main()
{

//      /* Check for the existence of RAM at 0xa000 and add it to heap */
//      if (PEEK(0xa000) == POKE(0xa000, PEEK(0xa000)+1)) {
//              _heapadd ((void *) 0xa000, 0x2000);
//      }

#ifdef A2E
    clrscr();
#elif defined(VIC20)
    POKE(0x900f, 254);          /* Nice color scheme */
#elif defined(C64)
    char *border = (char *) 0xd020;
    char *background = (char *) 0xd021;
    *border = 6;
    *background = 7;
#endif

#ifdef CBM
    printchar(147);             /* Clear */
    printchar(28);              /* Red */
    printchar(18);              /* Reverse On */

    /* Disable RUNSTOP/RESTORE */
    POKE(808, 100);
#endif

    calllevel = 1;
    returnSP = RETSTACKSZ - 1;
    varsbegin = NULL;
    varsend = NULL;
    varslocal = NULL;
    program = NULL;
    current = NULL;

#ifdef A2E
    videomode(VIDEOMODE_80COL);
    revers(1);
    print("      ***    EIGHTBALL V0.5    ***     \n");
    print("      ***    (C)BOBBI, 2018    ***     \n\n");
    revers(0);
#elif defined(C64)
    print("      ***    EightBall v0.5    ***      ");
    print("      ***    (c)Bobbi, 2018    ***      \n\n");
#elif defined(VIC20)
    /* Looks great in 22 cols! */
    print("*** EightBall v0.4 ****** (c)Bobbi, 2017 ***\n\n");
#else
    print("      ***    EightBall v0.5    ***     \n");
    print("      ***    (c)Bobbi, 2018    ***     \n\n");
#endif

#ifdef CBM
    printchar(144);             /* Black */
    printchar(146);             /* Reverse Off */
#endif
    print("Free Software.\n");
    print("Licenced under GPL.\n\n");

    CLEARHEAP1();
#ifdef CC65
    CLEARHEAP2();
#endif

    /* Warm reset goes here */
    if (setjmp(jumpbuf) == 1) {
        print("Restart\n");
    }

    for (;;) {
        clearexprstacks();
        if (editmode) {
#ifdef CBM
            printchar(30);      /* Green */
            printchar(18);      /* Reverse On */
#endif
            printchar('>');
#ifdef CBM
            printchar(144);     /* Black */
            printchar(146);     /* Reverse Off */
#endif
        }

        getln(lnbuf, 255);

        switch (editmode) {
        case 0:                /* Not editing - immediate mode execute */
            txtPtr = lnbuf;
            current = NULL;
            counter = -1;
            switch (parseline()) {
            case 0:
            case 1:
                printchar('\n');
                break;
            case 2:
                print(" err\n");
                returnSP = (RETSTACKSZ - 1);
                skipFlag = 0;
                break;
            case 3:
                print("Brk\n");
                returnSP = (RETSTACKSZ - 1);
                skipFlag = 0;
                break;
            }
            if (returnSP != (RETSTACKSZ - 1)) {
                error(ERR_STACK);
                returnSP = (RETSTACKSZ - 1);
            }
            skipFlag = 0;
            break;
        case 1:                /* Editing the program, period to escape */
            if (lnbuf[0] == '.') {
                editmode = 0;
            } else {
                appendline(lnbuf);
            }
            break;
        case 2:                /* Special case for insert first line */
            insertfirstline(lnbuf);
            findline(1);
            editmode = 1;
            break;
        }
    }
}
