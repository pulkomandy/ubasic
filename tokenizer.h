/*
 * Copyright (c) 2006, Adam Dunkels
 * All rights reserved.
 *
 * Copyright (c) 2015, Alan Cox
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

#define TOKENIZER_ERROR		((uint8_t)128)		/* Base of tokens */
#define TOKENIZER_ENDOFINPUT	((uint8_t)129)
#define TOKENIZER_LET		((uint8_t)130)
#define TOKENIZER_PRINT		((uint8_t)131)
#define TOKENIZER_IF		((uint8_t)132)
#define TOKENIZER_THEN		((uint8_t)133)
#define TOKENIZER_ELSE		((uint8_t)134)
#define TOKENIZER_FOR		((uint8_t)135)
#define TOKENIZER_TO		((uint8_t)136)
#define TOKENIZER_NEXT		((uint8_t)137)
#define TOKENIZER_STEP		((uint8_t)138)
#define TOKENIZER_GO		((uint8_t)139)
#define TOKENIZER_SUB		((uint8_t)140)
#define TOKENIZER_RETURN	((uint8_t)141)
#define TOKENIZER_CALL		((uint8_t)142)
#define TOKENIZER_REM		((uint8_t)143)
#define TOKENIZER_POKE		((uint8_t)144)
#define TOKENIZER_STOP		((uint8_t)145)
#define TOKENIZER_DATA		((uint8_t)146)
#define TOKENIZER_RANDOMIZE	((uint8_t)147)
#define TOKENIZER_OPTION	((uint8_t)148)
#define TOKENIZER_BASE		((uint8_t)149)
#define TOKENIZER_INPUT		((uint8_t)150)
#define TOKENIZER_RESTORE	((uint8_t)151)
#define TOKENIZER_TAB		((uint8_t)152)
#define TOKENIZER_NE		((uint8_t)153)
#define TOKENIZER_GE		((uint8_t)154)
#define TOKENIZER_LE		((uint8_t)155)
#define TOKENIZER_DIM		((uint8_t)156)
#define TOKENIZER_NUMBER	((uint8_t)192)	/* Numeric expression types */
#define TOKENIZER_INTVAR	((uint8_t)193)
#define TOKENIZER_PEEK		((uint8_t)194)
#define TOKENIZER_INT		((uint8_t)195)
#define TOKENIZER_ABS		((uint8_t)196)
#define TOKENIZER_SGN		((uint8_t)197)
#define TOKENIZER_LEN		((uint8_t)198)
#define TOKENIZER_CODE		((uint8_t)199)
#define TOKENIZER_VAL		((uint8_t)200)
#define TOKENIZER_STRING	((uint8_t)224)	/* String expression types */
#define TOKENIZER_STRINGVAR	((uint8_t)225)
#define TOKENIZER_LEFTSTR	((uint8_t)226)
#define TOKENIZER_RIGHTSTR	((uint8_t)227)
#define TOKENIZER_MIDSTR	((uint8_t)228)
#define TOKENIZER_CHRSTR	((uint8_t)229)
  /* Tokens that are single symbol assigned to themselves for efficiency */
#define TOKENIZER_COMMA		((uint8_t)',')
#define TOKENIZER_SEMICOLON	((uint8_t)';')
#define TOKENIZER_PLUS		((uint8_t)'+')
#define TOKENIZER_MINUS		((uint8_t)'-')
#define TOKENIZER_AND		((uint8_t)'&')
#define TOKENIZER_OR		((uint8_t)'|')
#define TOKENIZER_ASTR		((uint8_t)'*')
#define TOKENIZER_SLASH		((uint8_t)'/')
#define TOKENIZER_MOD		((uint8_t)'%')
#define TOKENIZER_HASH		((uint8_t)'#')
#define TOKENIZER_LEFTPAREN	((uint8_t)'(')
#define TOKENIZER_RIGHTPAREN	((uint8_t)')')
#define TOKENIZER_LT		((uint8_t)'<')
#define TOKENIZER_GT		((uint8_t)'>')
#define TOKENIZER_EQ		((uint8_t)'=')
#define TOKENIZER_POWER		((uint8_t)'^')
#define TOKENIZER_COLON		((uint8_t)':')
#define TOKENIZER_QUESTION 	((uint8_t)'?')
#define TOKENIZER_CR		((uint8_t)'\n')


#define TOKENIZER_NUMEXP(x)		(((x) & 0xE0) == 0xC0)
#define TOKENIZER_STRINGEXP(x)		(((x) & 0xE0) == 0xE0)

#define STRINGFLAG	0x8000
#define ARRAYFLAG	0x4000

typedef void (*stringfunc_t)(char c, void *ctx);
void tokenizer_goto(const char *program);
void tokenizer_init(const char *program);
void tokenizer_next(void);
void tokenizer_newline(void);
extern uint8_t current_token;
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
