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

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINTF(...)  printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "ubasic.h"
#include "tokenizer.h"

static char const *ptr, *nextptr;
static char const *saved_ptr, *saved_next;
static int saved_token;


#define MAX_NUMLEN 6

struct keyword_token {
  char *keyword;
  int token;
};

static int current_token = TOKENIZER_ERROR;

static const struct keyword_token keywords[] = {
  {"let", TOKENIZER_LET},
  {"print", TOKENIZER_PRINT},
  {"if", TOKENIZER_IF},
  {"then", TOKENIZER_THEN},
  {"else", TOKENIZER_ELSE},
  {"for", TOKENIZER_FOR},
  {"to", TOKENIZER_TO},
  {"next", TOKENIZER_NEXT},
  {"step", TOKENIZER_STEP},
  {"go", TOKENIZER_GO},
  {"sub", TOKENIZER_SUB},
  {"return", TOKENIZER_RETURN},
  {"call", TOKENIZER_CALL},
  {"rem", TOKENIZER_REM},
  {"poke", TOKENIZER_POKE},
  {"peek", TOKENIZER_PEEK},
  {"int", TOKENIZER_INT},
  {"abs", TOKENIZER_ABS},
  {"sgn", TOKENIZER_SGN},
  {"len", TOKENIZER_LEN},
  {"code", TOKENIZER_CODE},
  {"val", TOKENIZER_VAL},
  {"stop", TOKENIZER_STOP},
  {"and", TOKENIZER_AND},
  {"or", TOKENIZER_OR},
/* FIXME  {"not", TOKENIZER_NOT}, */
  {"data", TOKENIZER_DATA},
  {"randomize", TOKENIZER_RANDOMIZE},
  {"option", TOKENIZER_OPTION},
  {"base", TOKENIZER_BASE},
  {"input", TOKENIZER_INPUT},
  {"restore", TOKENIZER_RESTORE},
  {"tab", TOKENIZER_TAB},
  {"left$", TOKENIZER_LEFTSTR},
  {"right$", TOKENIZER_RIGHTSTR},
  {"mid$", TOKENIZER_MIDSTR},
  {"chr$", TOKENIZER_CHRSTR},
  {NULL, TOKENIZER_ERROR}
};

/*---------------------------------------------------------------------------*/
static uint8_t doublechar(void)
{
  /* Special case the paired single char symbols */
  if (*ptr == '>' && ptr[1] == '=')
    return TOKENIZER_GE;
  if (*ptr == '<' && ptr[1] == '=')
    return TOKENIZER_LE;
  if (*ptr == '<' && ptr[1] == '>')
    return TOKENIZER_NE;
  if (*ptr == '*' && ptr[1] == '*')
    return TOKENIZER_POWER;
  return 0;
}

/*---------------------------------------------------------------------------*/
static uint8_t singlechar(void)
{
  if (strchr("\n,;+-&|*/%(#)<>=^", *ptr))
    return *ptr;
  /* Not semantically meaningful */
  return 0;
}
/*---------------------------------------------------------------------------*/
static int get_next_token(void)
{
  struct keyword_token const *kt;
  int i;
  uint8_t t;

  DEBUG_PRINTF("get_next_token(): '%s'\n", ptr);

  if(*ptr == 0) {
    return TOKENIZER_ENDOFINPUT;
  }

  if ((isdigit(*ptr)) || (*ptr == '-' && isdigit(ptr[1]))) {
    i = 0;
    if (*ptr == '-')
      i = 1;
    for(; i < MAX_NUMLEN; ++i) {
      if(!isdigit(ptr[i])) {
        if(i > 0) {
          nextptr = ptr + i;
          return TOKENIZER_NUMBER;
        } else {
          DEBUG_PRINTF("get_next_token: error due to too short number\n");
          return TOKENIZER_ERROR;
        }
      }
      if(!isdigit(ptr[i])) {
        DEBUG_PRINTF("get_next_token: error due to malformed number\n");
        return TOKENIZER_ERROR;
      }
    }
    DEBUG_PRINTF("get_next_token: error due to too long number\n");
    return TOKENIZER_ERROR;
  } else if((t = doublechar()) != 0) {
    nextptr = ptr + 2;
    return t;
  } else if((t = singlechar()) != 0) {
    nextptr = ptr + 1;
    return t;
  } else if(*ptr == '"') {
    nextptr = ptr;
    do {
      ++nextptr;
      if (!*nextptr || *nextptr == '\n')
        ubasic_tokenizer_error();
    } while(*nextptr != '"');
    ++nextptr;
    return TOKENIZER_STRING;
  } else {
    for(kt = keywords; kt->keyword != NULL; ++kt) {
      if(strncasecmp(ptr, kt->keyword, strlen(kt->keyword)) == 0) {
        nextptr = ptr + strlen(kt->keyword);
        return kt->token;
      }
    }
  }

  if ((*ptr >= 'a' && *ptr <= 'z') || (*ptr >= 'A' && *ptr <= 'Z')) {
    nextptr = ptr + 1;
    if (*nextptr == '$') {
      nextptr++;
      return TOKENIZER_STRINGVAR;
    }
    if (isdigit(*nextptr))	/* A0-A9/B0-B9/etc */
      nextptr++;
    return TOKENIZER_INTVAR;
  }


  return TOKENIZER_ERROR;
}
/*---------------------------------------------------------------------------*/
void tokenizer_goto(const char *program)
{
  ptr = program;
  current_token = get_next_token();
}
/*---------------------------------------------------------------------------*/
void tokenizer_init(const char *program)
{
  tokenizer_goto(program);
  current_token = get_next_token();
}
/*---------------------------------------------------------------------------*/
int tokenizer_token(void)
{
  return current_token;
}
/*---------------------------------------------------------------------------*/
void tokenizer_push(void)
{
  saved_ptr = ptr;
  saved_next = nextptr;
  saved_token = current_token;
}
/*---------------------------------------------------------------------------*/
void tokenizer_pop(void)
{
  ptr = saved_ptr;
  nextptr = saved_next;
  current_token = saved_token;
}
/*---------------------------------------------------------------------------*/
void tokenizer_next(void)
{

  if(tokenizer_finished()) {
    return;
  }

  DEBUG_PRINTF("tokenizer_next: %p\n", nextptr);
  ptr = nextptr;

  while(*ptr == ' ') {
    ++ptr;
  }
  current_token = get_next_token();

  DEBUG_PRINTF("tokenizer_next: '%s' %d\n", ptr, current_token);
  return;
}

/*---------------------------------------------------------------------------*/

void tokenizer_newline(void)
{
   while(!(*nextptr == '\n' || tokenizer_finished())) {
     ++nextptr;
  }
  if(*nextptr == '\n') {
    ++nextptr;
  }
  tokenizer_next();
}

/*---------------------------------------------------------------------------*/
value_t tokenizer_num(void)
{
  return atoi(ptr);
}
/*---------------------------------------------------------------------------*/
int tokenizer_string_len(void)
{
  char *string_end;

  if(tokenizer_token() != TOKENIZER_STRING) {
    fprintf(stderr, "strlbotch");
    exit(1);
  }
  string_end = strchr(ptr + 1, '"');
  /* Pass -1 back so we can keep the notional split between the tokenizer
     and core code cleaner */
  if(string_end == NULL)
    ubasic_tokenizer_error();
  return string_end - ptr - 1;
}

/*---------------------------------------------------------------------------*/
char const *tokenizer_string(void)
{
  return ptr + 1;
}

/*---------------------------------------------------------------------------*/
void tokenizer_string_func(stringfunc_t func, void *ctx)
{
  const char *string_end, *p;

  if(tokenizer_token() != TOKENIZER_STRING) {
    return;
  }
  p = ptr + 1;
  string_end = strchr(p, '"');
  if(string_end == NULL)
    ubasic_tokenizer_error();
  while(p != string_end)
    func(*p++, ctx);
}

/*---------------------------------------------------------------------------*/
void tokenizer_error_print(void)
{
  ubasic_tokenizer_error();
}
/*---------------------------------------------------------------------------*/
int
tokenizer_finished(void)
{
  return *ptr == 0 || current_token == TOKENIZER_ENDOFINPUT;
}
/*---------------------------------------------------------------------------*/
int
tokenizer_variable_num(void)
{
  if (ptr[1] == '$')
    return STRINGFLAG | (toupper(*ptr) - 'A');
  /* FIXME: hard code to use &~0x20 as we already know it is a letter */
  if (!isdigit(ptr[1]))
    return toupper(*ptr) - 'A';
  else {
    /* One day we'll need long vars and brains, until then.. */
    return (toupper(*ptr) - '@') * 11 + ptr[1] - '0';
  }
}
/*---------------------------------------------------------------------------*/
char const *
tokenizer_pos(void)
{
    return ptr;
}
