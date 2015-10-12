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
#ifndef __UBASIC_H__
#define __UBASIC_H__

typedef uint16_t	line_t;
typedef int16_t		value_t;
typedef uint16_t	var_t;

typedef value_t (*peek_func)(value_t);
typedef void (*poke_func)(value_t, value_t);

enum type {
  TYPE_INTEGER = 'I',
  TYPE_STRING = 'S'
};

struct typevalue {
  enum type type;
  union {
    value_t i;
    uint8_t *p;
  } d;
};


void ubasic_init(const char *program);
void ubasic_init_peek_poke(const char *program, peek_func peek, poke_func poke);
void ubasic_run(void);
void ubasic_tokenizer_error(void);
int ubasic_finished(void);

extern line_t line_num;

void ubasic_get_variable(int varnum, struct typevalue *v);
void ubasic_set_variable(int varum, struct typevalue *value);

#endif /* __UBASIC_H__ */
