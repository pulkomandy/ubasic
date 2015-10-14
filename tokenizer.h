/*
 * Copyright (c) 2006, Adam Dunkels
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef __TOKENIZER_H__
#define __TOKENIZER_H__

enum {
  TOKENIZER_ERROR = 128,	/* Base of tokens */
  TOKENIZER_ENDOFINPUT,
  TOKENIZER_LET,
  TOKENIZER_PRINT,
  TOKENIZER_IF,
  TOKENIZER_THEN,
  TOKENIZER_ELSE,
  TOKENIZER_FOR,
  TOKENIZER_TO,
  TOKENIZER_NEXT,
  TOKENIZER_STEP,
  TOKENIZER_GO,
  TOKENIZER_SUB,
  TOKENIZER_RETURN,
  TOKENIZER_CALL,
  TOKENIZER_REM,
  TOKENIZER_POKE,
  TOKENIZER_STOP,
  TOKENIZER_DATA,
  TOKENIZER_RANDOMIZE,
  TOKENIZER_OPTION,
  TOKENIZER_BASE,
  TOKENIZER_INPUT,
  TOKENIZER_RESTORE,
  TOKENIZER_TAB,
  TOKENIZER_NE,
  TOKENIZER_GE,
  TOKENIZER_LE,
  TOKENIZER_DIM,
  TOKENIZER_NUMBER = 192,	/* Numeric expression types */
  TOKENIZER_INTVAR,
  TOKENIZER_PEEK,	
  TOKENIZER_INT,
  TOKENIZER_ABS,
  TOKENIZER_SGN,
  TOKENIZER_LEN,
  TOKENIZER_CODE,
  TOKENIZER_VAL,
  TOKENIZER_STRING = 224,	/* String expression types */
  TOKENIZER_STRINGVAR,
  TOKENIZER_LEFTSTR,
  TOKENIZER_RIGHTSTR,
  TOKENIZER_MIDSTR,
  TOKENIZER_CHRSTR,
  /* Tokens that are single symbol assigned to themselves for efficiency */
  TOKENIZER_COMMA = ',',
  TOKENIZER_SEMICOLON = ';',
  TOKENIZER_PLUS = '+',
  TOKENIZER_MINUS = '-',
  TOKENIZER_AND = '&',
  TOKENIZER_OR = '|',
  TOKENIZER_ASTR = '*',
  TOKENIZER_SLASH = '/',
  TOKENIZER_MOD = '%',
  TOKENIZER_HASH = '#',
  TOKENIZER_LEFTPAREN = '(',
  TOKENIZER_RIGHTPAREN = ')',
  TOKENIZER_LT = '<',
  TOKENIZER_GT = '>',
  TOKENIZER_EQ = '=',
  TOKENIZER_POWER = '^',
  TOKENIZER_COLON = ':',
  TOKENIZER_CR = '\n'
};

#define TOKENIZER_NUMEXP(x)		(((x) & 0xE0) == 0xC0)
#define TOKENIZER_STRINGEXP(x)		(((x) & 0xE0) == 0xE0)

#define STRINGFLAG	0x8000
#define ARRAYFLAG	0x4000

typedef void (*stringfunc_t)(char c, void *ctx);
void tokenizer_goto(const char *program);
void tokenizer_init(const char *program);
void tokenizer_next(void);
void tokenizer_newline(void);
int tokenizer_token(void);
value_t tokenizer_num(void);
int tokenizer_variable_num(void);
char const *tokenizer_string(void);
int tokenizer_string_len(void);
void tokenizer_string_func(stringfunc_t func, void *ctx);
void tokenizer_push(void);
void tokenizer_pop(void);
int tokenizer_finished(void);
void tokenizer_error_print(void);

char const *tokenizer_pos(void);

#endif /* __TOKENIZER_H__ */
