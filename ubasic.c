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

#define DEBUG 0

#if DEBUG
#define DEBUG_PRINTF(...)  printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#include <stdio.h> /* printf() */
#include <stdlib.h> /* exit() */
#include <stdint.h> /* Types */
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ubasic.h"
#include "tokenizer.h"

static char const *program_ptr;

#define MAX_GOSUB_STACK_DEPTH 10
static line_t gosub_stack[MAX_GOSUB_STACK_DEPTH];
static int gosub_stack_ptr;

struct for_state {
  line_t line_after_for;
  var_t for_variable;
  value_t to;
  value_t step;
};

#define MAX_FOR_STACK_DEPTH 4
static struct for_state for_stack[MAX_FOR_STACK_DEPTH];
static int for_stack_ptr;

struct line_index {
  line_t line_number;
  char const *program_text_position;
  struct line_index *next;
};
struct line_index *line_index_head = NULL;
struct line_index *line_index_current = NULL;

#define MAX_VARNUM 26 * 11
static value_t variables[MAX_VARNUM];
#define MAX_STRING 26
static uint8_t *strings[MAX_STRING];

static int ended;

static void expr(struct typevalue *val);
static void line_statement(void);
static void statement(void);
static void index_free(void);

peek_func peek_function = NULL;
poke_func poke_function = NULL;

line_t line_num;
static const char *data_position;
static int data_seek;

static unsigned int array_base = 0;

/*---------------------------------------------------------------------------*/
void ubasic_init(const char *program)
{
  program_ptr = program;
  for_stack_ptr = gosub_stack_ptr = 0;
  index_free();
  tokenizer_init(program);
  data_position = program_ptr;
  data_seek = 1;
  ended = 0;
}
/*---------------------------------------------------------------------------*/
void ubasic_init_peek_poke(const char *program, peek_func peek, poke_func poke)
{
  program_ptr = program;
  for_stack_ptr = gosub_stack_ptr = 0;
  index_free();
  peek_function = peek;
  poke_function = poke;
  tokenizer_init(program);
  ended = 0;
}
/*---------------------------------------------------------------------------*/
static uint8_t accept_tok(uint8_t token)
{
  if(token != tokenizer_token()) {
    DEBUG_PRINTF("Token not what was expected (expected %d, got %d)\n",
                token, tokenizer_token());
    tokenizer_error_print();
    exit(1);
  }
  DEBUG_PRINTF("Expected %d, got it\n", token);
  tokenizer_next();
  /* This saves lots of extra calls - return the new token */
  return tokenizer_token();
}
/*---------------------------------------------------------------------------*/
static uint8_t accept_either(uint8_t tok1, uint8_t tok2)
{
  uint8_t t = tokenizer_token();
  if (t == tok2)
    accept_tok(tok2);
  else
    accept_tok(tok1);
  return t;
}

static void bracketed_expr(struct typevalue *v)
{
  accept_tok(TOKENIZER_LEFTPAREN);
  expr(v);
  accept_tok(TOKENIZER_RIGHTPAREN);
}

/*---------------------------------------------------------------------------*/
static void typecheck_int(struct typevalue *v)
{
  if (v->type != TYPE_INTEGER) {
    fprintf(stderr, "Line %d Integer required\n", line_num);
    exit(1);
  }
}
/*---------------------------------------------------------------------------*/
static void typecheck_string(struct typevalue *v)
{
  if (v->type != TYPE_STRING) {
    fprintf(stderr, "Line %d String required\n", line_num);
    exit(1);
  }
}
/*---------------------------------------------------------------------------*/
static void typecheck_same(struct typevalue *l, struct typevalue *r)
{
  if (l->type != r->type) {
    fprintf(stderr, "Line %d Type mismatch\n", line_num);
    exit(1);
  }
}
/*---------------------------------------------------------------------------*/
/* Temoporary implementation of string workspaces */

static uint8_t stringblob[512];
static uint8_t *nextstr;

static uint8_t *string_temp(int len)
{
  uint8_t *p = nextstr;
  if (len > 255) {
    fprintf(stderr, "Line %d String too long\n", line_num);
    exit(1);
  }
  nextstr += len + 1;
  if (nextstr > stringblob + sizeof(stringblob)) {
    fprintf(stderr, "Line %d Out of string temporaries\n", line_num);
    exit(1);
  }
  *p = len;
  return p;
}

static void string_temp_free(void)
{
  nextstr = stringblob;
}

/*---------------------------------------------------------------------------*/

static value_t bracketed_intexpr(void)
{
  struct typevalue v;
  bracketed_expr(&v);
  typecheck_int(&v);
  return v.d.i;
}
/*---------------------------------------------------------------------------*/
static void varfactor(struct typevalue *v)
{
  ubasic_get_variable(tokenizer_variable_num(), v);
  DEBUG_PRINTF("varfactor: obtaining %d from variable %d\n", v->d.i, tokenizer_variable_num());
  accept_either(TOKENIZER_INTVAR, TOKENIZER_STRINGVAR);
}
/*---------------------------------------------------------------------------*/
static void factor(struct typevalue *v)
{
  uint8_t t = tokenizer_token();
  int len;

  DEBUG_PRINTF("factor: token %d\n", tokenizer_token());
  switch(t) {
  case TOKENIZER_STRING:
    /* FIXME - allocate/copy */
    v->type = TYPE_STRING;
    len = tokenizer_string_len();
    v->d.p = string_temp(len);
    memcpy(v->d.p + 1, tokenizer_string(), len);
    DEBUG_PRINTF("factor: string %p\n", v->d.p);
    accept_tok(TOKENIZER_STRING);
    break;
  case TOKENIZER_NUMBER:
    v->d.i = tokenizer_num();
    v->type = TYPE_INTEGER;
    DEBUG_PRINTF("factor: number %d\n", v->d.i);
    accept_tok(TOKENIZER_NUMBER);
    break;
  case TOKENIZER_LEFTPAREN:
    accept_tok(TOKENIZER_LEFTPAREN);
    expr(v);
    accept_tok(TOKENIZER_RIGHTPAREN);
    break;
  case TOKENIZER_INTVAR:
  case TOKENIZER_STRINGVAR:
    varfactor(v);
    break;
  default:
    if (TOKENIZER_NUMEXP(t)) {
      accept_tok(t);
      bracketed_expr(v);
      typecheck_int(v);
      /* Check v.type at some point */
      switch(t) {
      case TOKENIZER_PEEK:
        v->d.i = peek_function(v->d.i);
        break;
      case TOKENIZER_ABS:
        if (v->d.i < 0)
          v->d.i = -v->d.i;
        break;
      case TOKENIZER_INT:
        break;
      case TOKENIZER_SGN:
        if (v->d.i > 1 ) v->d.i = 1;
        if (v->d.i < 0) v->d.i = -1;
        break;
      default:
        fprintf(stderr, "BOGOEXP\n");
      }
    }
    else {
      fprintf(stderr, "Line %d Syntax Error\n", line_num);
      exit(1);
    }
  }
}

/*---------------------------------------------------------------------------*/
static void term(struct typevalue *v)
{
  struct typevalue f2;
  int op;

  factor(v);
  op = tokenizer_token();
  DEBUG_PRINTF("term: token %d\n", op);
  while(op == TOKENIZER_ASTR ||
       op == TOKENIZER_SLASH ||
       op == TOKENIZER_MOD) {
    tokenizer_next();
    factor(&f2);
    typecheck_int(v);
    typecheck_int(&f2);
    DEBUG_PRINTF("term: %d %d %d\n", v->d.i, op, f2.d.i);
    switch(op) {
    case TOKENIZER_ASTR:
      v->d.i *= f2.d.i;
      break;
    case TOKENIZER_SLASH:
      if (f2.d.i == 0) {
        fprintf(stderr, "Line %d Division by zero\n", line_num);
        exit(1);
      }
      v->d.i /= f2.d.i;
      break;
    case TOKENIZER_MOD:
      if (f2.d.i == 0) {
        fprintf(stderr, "Line %d Mod by zero\n", line_num);
        exit(1);
      }
      v->d.i %= f2.d.i;
      break;
    }
    op = tokenizer_token();
  }
  DEBUG_PRINTF("term: %d\n", v->d.i);
}
/*---------------------------------------------------------------------------*/
static void expr(struct typevalue *v)
{
  struct typevalue t2;
  int op;

  term(v);
  op = tokenizer_token();
  DEBUG_PRINTF("expr: token %d\n", op);
  while(op == TOKENIZER_PLUS ||
       op == TOKENIZER_MINUS ||
       op == TOKENIZER_AND ||
       op == TOKENIZER_OR) {
    tokenizer_next();
    term(&t2);
    if (op != TOKENIZER_PLUS)
      typecheck_int(v);
    typecheck_same(v, &t2);
    DEBUG_PRINTF("expr: %d %d %d\n", v->d.i, op, t2.d.i);
    switch(op) {
    case TOKENIZER_PLUS:
      if (v->type == TYPE_INTEGER)
        v->d.i += t2.d.i;
      else {
        uint8_t *p;
        uint8_t l = *v->d.p;
        p = string_temp(l + *t2.d.p);
        memcpy(p + 1, v->d.p + 1, l);
        memcpy(p + l + 1, t2.d.p + 1, *t2.d.p);
        v->d.p = p;
      }
      break;
    case TOKENIZER_MINUS:
      v->d.i -= t2.d.i;
      break;
    case TOKENIZER_AND:
      v->d.i &= t2.d.i;
      break;
    case TOKENIZER_OR:
      v->d.i |= t2.d.i;
      break;
    }
    op = tokenizer_token();
  }
  DEBUG_PRINTF("expr: %d\n", v->d.i);
}
/*---------------------------------------------------------------------------*/
static void relation(struct typevalue *r1)
{
  struct typevalue r2;
  int op;

  expr(r1);
  op = tokenizer_token();
  DEBUG_PRINTF("relation: token %d\n", op);
  while(op == TOKENIZER_LT ||
       op == TOKENIZER_GT ||
       op == TOKENIZER_EQ) {
    tokenizer_next();
    expr(&r2);
    typecheck_same(r1, &r2);
    DEBUG_PRINTF("relation: %d %d %d\n", r1->d.i, op, r2.d.i);
    switch(op) {
    /* FIXME: string versions of these four */
    case TOKENIZER_LT:
      r1->d.i = r1->d.i < r2.d.i;
      break;
    case TOKENIZER_GT:
      r1->d.i = r1->d.i > r2.d.i;
      break;
    case TOKENIZER_EQ:
      r1->d.i = r1->d.i == r2.d.i;
      break;
    }
    op = tokenizer_token();
  }
  r1->type = TYPE_INTEGER;
}
/*---------------------------------------------------------------------------*/
static value_t intexpr(void)
{
  struct typevalue t;
  expr(&t);
  typecheck_int(&t);
  return t.d.i;
}
/*---------------------------------------------------------------------------*/
static uint8_t *stringexpr(void)
{
  struct typevalue t;
  expr(&t);
  typecheck_string(&t);
  return t.d.p;
}
/*---------------------------------------------------------------------------*/
static void index_free(void) {
  if(line_index_head != NULL) {
    line_index_current = line_index_head;
    do {
      DEBUG_PRINTF("Freeing index for line %p.\n", (void *)line_index_current);
      line_index_head = line_index_current;
      line_index_current = line_index_current->next;
      free(line_index_head);
    } while (line_index_current != NULL);
    line_index_head = NULL;
  }
}
/*---------------------------------------------------------------------------*/
static char const*index_find(int linenum) {
  #if DEBUG
  int step = 0;
  #endif
  struct line_index *lidx;
  lidx = line_index_head;


  while(lidx != NULL && lidx->line_number != linenum) {
    lidx = lidx->next;

    #if DEBUG
    if(lidx != NULL) {
      DEBUG_PRINTF("index_find: Step %3d. Found index for line %d: %p.\n",
                   step, lidx->line_number,
                   lidx->program_text_position);
    }
    step++;
    #endif
  }
  if(lidx != NULL && lidx->line_number == linenum) {
    DEBUG_PRINTF("index_find: Returning index for line %d.\n", linenum);
    return lidx->program_text_position;
  }
  DEBUG_PRINTF("index_find: Returning NULL.\n");
  return NULL;
}
/*---------------------------------------------------------------------------*/
static void index_add(int linenum, char const* sourcepos) {
  struct line_index *new_lidx;

  if(line_index_head != NULL && index_find(linenum)) {
    return;
  }

  new_lidx = malloc(sizeof(struct line_index));
  new_lidx->line_number = linenum;
  new_lidx->program_text_position = sourcepos;
  new_lidx->next = NULL;

  if(line_index_head != NULL) {
    line_index_current->next = new_lidx;
    line_index_current = line_index_current->next;
  } else {
    line_index_current = new_lidx;
    line_index_head = line_index_current;
  }
  DEBUG_PRINTF("index_add: Adding index for line %d: %p.\n", linenum,
               sourcepos);
}
/*---------------------------------------------------------------------------*/
static void
jump_linenum_slow(int linenum)
{
  tokenizer_init(program_ptr);
  while(tokenizer_num() != linenum) {
    do {
      do {
        tokenizer_next();
      } while(tokenizer_token() != TOKENIZER_CR &&
          tokenizer_token() != TOKENIZER_ENDOFINPUT);
      if(tokenizer_token() == TOKENIZER_CR) {
        tokenizer_next();
      }
    } while(tokenizer_token() != TOKENIZER_NUMBER);
    DEBUG_PRINTF("jump_linenum_slow: Found line %d\n", tokenizer_num());
  }
}
/*---------------------------------------------------------------------------*/
static void
jump_linenum(int linenum)
{
  char const* pos = index_find(linenum);
  if(pos != NULL) {
    DEBUG_PRINTF("jump_linenum: Going to line %d.\n", linenum);
    tokenizer_goto(pos);
  } else {
    /* We'll try to find a yet-unindexed line to jump to. */
    DEBUG_PRINTF("jump_linenum: Calling jump_linenum_slow %d.\n", linenum);
    jump_linenum_slow(linenum);
  }
}
/*---------------------------------------------------------------------------*/
static void go_statement(void)
{
  int linenum;
  uint8_t t;

  accept_tok(TOKENIZER_GO);
  t = accept_either(TOKENIZER_TO, TOKENIZER_SUB);
  if (t == TOKENIZER_TO) {
    linenum = intexpr();
    accept_tok(TOKENIZER_CR);
    jump_linenum(linenum);
    return;
  }
  linenum = intexpr();
  accept_tok(TOKENIZER_CR);
  if(gosub_stack_ptr < MAX_GOSUB_STACK_DEPTH) {
    gosub_stack[gosub_stack_ptr] = tokenizer_num();
    gosub_stack_ptr++;
    jump_linenum(linenum);
  } else {
    DEBUG_PRINTF("gosub_statement: gosub stack exhausted\n");
    /* FIXME: error here */
  }
}
/*---------------------------------------------------------------------------*/

static int chpos = 0;

static void charout(char c, void *unused)
{
  if (c == '\t') {
    do {
      charout(' ', NULL);
    } while(chpos%8);
    return;
  }
  putchar(c);
  if ((c == 8 || c== 127) && chpos)
    chpos--;
  else if (c == '\r' || c == '\n')
    chpos = 0;
  else
    chpos++;
}

static void charreset(void)
{
  chpos = 0;
}

static void chartab(value_t v)
{
  while(chpos < v)
    charout(' ', NULL);
}

static void charoutstr(uint8_t *p)
{
  int len =*p++;
  while(len--)
    charout(*p++, NULL);
}

static void print_statement(void)
{
  uint8_t nonl;
  uint8_t t;

  accept_tok(TOKENIZER_PRINT);
  do {
    t = tokenizer_token();
    nonl = 0;
    DEBUG_PRINTF("Print loop\n");
    if(t == TOKENIZER_STRING) {
      /* Handle string const specially - length rules */
      tokenizer_string_func(charout, NULL);
      tokenizer_next();
    } else if(TOKENIZER_STRINGEXP(t)) {
      charoutstr(stringexpr());
    } else if(t == TOKENIZER_COMMA) {
      printf("\t");
      nonl = 1;
      tokenizer_next();
    } else if(t == TOKENIZER_SEMICOLON) {
      nonl = 1;
      tokenizer_next();
    } else if(TOKENIZER_NUMEXP(t)) {
      printf("%d", intexpr());
    } else if(t == TOKENIZER_TAB) {
      accept_tok(TOKENIZER_TAB);
      chartab(bracketed_intexpr());
    } else if (t != TOKENIZER_CR) {
      printf("TOK type %c\n", t);
      /* FIXME: Error out*/
      break;
    }
  } while(t != TOKENIZER_CR &&
      t != TOKENIZER_ENDOFINPUT);
  if (!nonl)
    printf("\n");
  DEBUG_PRINTF("End of print\n");
  tokenizer_next();
}
/*---------------------------------------------------------------------------*/
static void if_statement(void)
{
  struct typevalue r;

  accept_tok(TOKENIZER_IF);

  relation(&r);
  DEBUG_PRINTF("if_statement: relation %d\n", r.d.i);
  accept_tok(TOKENIZER_THEN);
  if(r.d.i) {
    statement();
  } else {
    do {
      tokenizer_next();
    } while(tokenizer_token() != TOKENIZER_ELSE &&
        tokenizer_token() != TOKENIZER_CR &&
        tokenizer_token() != TOKENIZER_ENDOFINPUT);
    if(tokenizer_token() == TOKENIZER_ELSE) {
      tokenizer_next();
      statement();
    } else if(tokenizer_token() == TOKENIZER_CR) {
      tokenizer_next();
    }
  }
}
/*---------------------------------------------------------------------------*/
static void let_statement(void)
{
  var_t var;
  struct typevalue v;

  var = tokenizer_variable_num();

  accept_either(TOKENIZER_INTVAR, TOKENIZER_STRINGVAR);
  accept_tok(TOKENIZER_EQ);
  expr(&v);
  DEBUG_PRINTF("let_statement: assign %d to %d\n", var, v.d.i);
  ubasic_set_variable(var, &v);
  accept_tok(TOKENIZER_CR);

}
/*---------------------------------------------------------------------------*/
static void return_statement(void)
{
  accept_tok(TOKENIZER_RETURN);
  if(gosub_stack_ptr > 0) {
    gosub_stack_ptr--;
    jump_linenum(gosub_stack[gosub_stack_ptr]);
  } else {
    DEBUG_PRINTF("return_statement: non-matching return\n");
  }
}
/*---------------------------------------------------------------------------*/
static void next_statement(void)
{
  int var;
  struct for_state *fs;
  struct typevalue t;

  /* FIXME: support 'NEXT' on its own, also loop down the stack so if you
     GOTO out of a layer of NEXT the right thing occurs */
  accept_tok(TOKENIZER_NEXT);
  var = tokenizer_variable_num();
  accept_tok(TOKENIZER_INTVAR);
  
  /* FIXME: make the for stack just use pointers so it compiles better */
  fs = &for_stack[for_stack_ptr - 1];
  if(for_stack_ptr > 0 &&
     var == fs->for_variable) {
    ubasic_get_variable(var, &t);
    t.d.i += fs->step;
    ubasic_set_variable(var, &t);
    /* NEXT end depends upon sign of STEP */
    if ((fs->step >= 0 && t.d.i <= fs->to) ||
        (fs->step < 0 && t.d.i >= fs->to)) {
      jump_linenum(fs->line_after_for);
    } else {
      for_stack_ptr--;
      accept_tok(TOKENIZER_CR);
    }
  } else {
    fprintf(stderr, "Line %d: mismatched NEXT\n", line_num);
    exit(1);
    DEBUG_PRINTF("next_statement: non-matching next (expected %d, found %d)\n", for_stack[for_stack_ptr - 1].for_variable, var);
    accept_tok(TOKENIZER_CR);
  }

}
/*---------------------------------------------------------------------------*/
static void for_statement(void)
{
  var_t for_variable;
  value_t to, step = 1;
  struct typevalue t;

  accept_tok(TOKENIZER_FOR);
  for_variable = tokenizer_variable_num();
  /* FIXME: typecheck the variable */
  accept_tok(TOKENIZER_INTVAR);
  accept_tok(TOKENIZER_EQ);
  expr(&t);
  typecheck_int(&t);
  ubasic_set_variable(for_variable, &t);
  accept_tok(TOKENIZER_TO);
  to = intexpr();
  if (tokenizer_token() == TOKENIZER_STEP) {
    accept_tok(TOKENIZER_STEP);
    step = intexpr();
  }
  accept_tok(TOKENIZER_CR);

  if(for_stack_ptr < MAX_FOR_STACK_DEPTH) {
    struct for_state *fs = &for_stack[for_stack_ptr];
    fs->line_after_for = tokenizer_num();
    fs->for_variable = for_variable;
    fs->to = to;
    fs->step = step;
    DEBUG_PRINTF("for_statement: new for, var %d to %d step %d\n",
                fs->for_variable,
                fs->to,
                fs->step);

    for_stack_ptr++;
  } else {
    DEBUG_PRINTF("for_statement: for stack depth exceeded\n");
  }
}
/*---------------------------------------------------------------------------*/
static void poke_statement(void)
{
  value_t poke_addr;
  value_t value;

  accept_tok(TOKENIZER_POKE);
  poke_addr = intexpr();
  accept_tok(TOKENIZER_COMMA);
  value = intexpr();
  accept_tok(TOKENIZER_CR);

  poke_function(poke_addr, value);
}
/*---------------------------------------------------------------------------*/
static void stop_statement(void)
{
  accept_tok(TOKENIZER_STOP);
  accept_tok(TOKENIZER_CR);
  ended = 1;
}
/*---------------------------------------------------------------------------*/
static void rem_statement(void)
{
  accept_tok(TOKENIZER_REM);
  tokenizer_newline();
}

/*---------------------------------------------------------------------------*/
static void data_statement(void)
{
  uint8_t t;
  accept_tok(TOKENIZER_DATA);
  do {
    t = tokenizer_token();
    /* We could just as easily allow expressions which might be wild... */
    /* Some platforms allow 4,,5  ... we don't yet FIXME */
    if (t == TOKENIZER_STRING || t == TOKENIZER_NUMBER)
      tokenizer_next();
    else {
      fprintf(stderr, "Line %d, syntax error\n", line_num);
      exit(1);
    }
    t = accept_either(TOKENIZER_CR, TOKENIZER_COMMA);
  } while(t != TOKENIZER_CR);
}

/*---------------------------------------------------------------------------*/
static void randomize_statement(void)
{
  value_t r = 0;
  accept_tok(TOKENIZER_RANDOMIZE);
  /* FIXME: replace all the CR checks with TOKENIZER_EOS() or similar so we
     can deal with ':' */
  if (tokenizer_token() != TOKENIZER_CR)
    r = intexpr();
  if (r)
    srand(getpid()^getuid()^time(NULL));
  else
    srand(r);
  accept_tok(TOKENIZER_CR);
}

/*---------------------------------------------------------------------------*/

static void option_statement(void)
{
  value_t r;
  accept_tok(TOKENIZER_OPTION);
  accept_tok(TOKENIZER_BASE);
  r = intexpr();
  accept_tok(TOKENIZER_CR);
  if (r < 0 || r > 1) {
    fprintf(stderr, "Line %d: Invalid base\n", line_num);
    exit(1);
  }
  array_base = r;
}

/*---------------------------------------------------------------------------*/

static void input_statement(void)
{
  struct typevalue r;
  var_t v;
  char buf[128];
  char *p;
  char *sp = buf;
  uint8_t t;
  
  accept_tok(TOKENIZER_INPUT);

  t = tokenizer_token();
  if (TOKENIZER_STRINGEXP(t)) {
    tokenizer_string_func(charout, NULL);
    tokenizer_next();
    t = tokenizer_token();
    if (t == TOKENIZER_COMMA)
      accept_tok(TOKENIZER_COMMA);
    else
      accept_tok(TOKENIZER_SEMICOLON);	/* accept_tok_pair needed  ? */
  } else {
    charout('?', NULL);
    charout(' ', NULL);
  }
  if (fgets(buf, 128, stdin) == NULL) {
    fprintf(stderr, "EOF\n");
    exit(1);
  }
  charreset();		/* Newline input so move to left */

  /* Consider the single var allowed version of INPUT - it's saner for
     strings by far ? */
  do {  
    v = tokenizer_variable_num();
    accept_either(TOKENIZER_INTVAR, TOKENIZER_STRINGVAR);
  
    p = strtok(sp, " ,\t\n");
    sp = NULL;
    r.type = TYPE_INTEGER;	/* For now */
    if (p == NULL)
      r.d.i = 0;
    else
      r.d.i = atoi(p);	/* FIXME: error checking */
    ubasic_set_variable(v, &r);
    t = accept_either(TOKENIZER_CR, TOKENIZER_COMMA);
  } while(t != TOKENIZER_CR);
}

/*---------------------------------------------------------------------------*/

void restore_statement(void)
{
  int linenum = 0;
  uint8_t t;
  t = accept_tok(TOKENIZER_RESTORE);
  if (t != TOKENIZER_CR)
    linenum = intexpr();
  accept_tok(TOKENIZER_CR);
  if (linenum) {
    tokenizer_push();
    jump_linenum(linenum);
    data_position = tokenizer_pos();
    tokenizer_pop();
  } else
    data_position = program_ptr;
  data_seek = 1;
}

/*---------------------------------------------------------------------------*/
static void statement(void)
{
  int token;

  string_temp_free();

  token = tokenizer_token();

  switch(token) {
  case TOKENIZER_PRINT:
    print_statement();
    break;
  case TOKENIZER_IF:
    if_statement();
    break;
  case TOKENIZER_GO:
    go_statement();
    break;
  case TOKENIZER_RETURN:
    return_statement();
    break;
  case TOKENIZER_FOR:
    for_statement();
    break;
  case TOKENIZER_POKE:
    poke_statement();
    break;
  case TOKENIZER_NEXT:
    next_statement();
    break;
  case TOKENIZER_STOP:
    stop_statement();
    break;
  case TOKENIZER_REM:
    rem_statement();
    break;
  case TOKENIZER_DATA:
    data_statement();
    break;
  case TOKENIZER_RANDOMIZE:
    randomize_statement();
    break;
  case TOKENIZER_OPTION:
    option_statement();
    break;
  case TOKENIZER_INPUT:
    input_statement();
    break;
  case TOKENIZER_RESTORE:
    restore_statement();
    break;
  case TOKENIZER_LET:
    accept_tok(TOKENIZER_LET);
    /* Fall through. */
  case TOKENIZER_STRINGVAR:
  case TOKENIZER_INTVAR:
    let_statement();
    break;
  default:
    DEBUG_PRINTF("ubasic.c: statement(): not implemented %d\n", token);
    if (line_num)
      fprintf(stderr, "Line %d ", line_num);
    fprintf(stderr, "Syntax error\n");
    exit(1);
  }
}
/*---------------------------------------------------------------------------*/
static void line_statement(void)
{
  line_num = tokenizer_num();
  DEBUG_PRINTF("----------- Line number %d ---------\n", line_num);
  index_add(line_num, tokenizer_pos());
  accept_tok(TOKENIZER_NUMBER);
  statement();
  return;
}
/*---------------------------------------------------------------------------*/
void ubasic_run(void)
{
  if(tokenizer_finished()) {
    DEBUG_PRINTF("uBASIC program finished\n");
    return;
  }

  line_statement();
}
/*---------------------------------------------------------------------------*/
int ubasic_finished(void)
{
  return ended || tokenizer_finished();
}
/*---------------------------------------------------------------------------*/

/* This helper will change once we try and stamp out malloc but will do for
   the moment */
static uint8_t *string_save(uint8_t *p)
{
  uint8_t *b = malloc(*p + 1);
  if (b == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
  memcpy(b, p, *p + 1);
  return b;
}

void ubasic_set_variable(int varnum, struct typevalue *value)
{
  if (varnum & STRINGFLAG) {
    typecheck_string(value);
    varnum &= ~STRINGFLAG;
    if (strings[varnum])
      free(strings[varnum]);
    strings[varnum] = string_save(value->d.p);
  } else {
    typecheck_int(value);
    if(varnum >= 0 && varnum <= MAX_VARNUM)
      variables[varnum] = value->d.i;
    else {
      fprintf(stderr, "BADVW\n");
      exit(1);
    }
  }
}
/*---------------------------------------------------------------------------*/
void ubasic_get_variable(int varnum, struct typevalue *value)
{
  if (varnum & STRINGFLAG) {
    value->d.p = strings[varnum & ~STRINGFLAG];
    value->type = TYPE_STRING;
  } else if(varnum >= 0 && varnum <= MAX_VARNUM) {
    value->d.i = variables[varnum];
    value->type = TYPE_INTEGER;
  } else {
    fprintf(stderr, "BADVAR\n");
    exit(1);
  }
}
/*---------------------------------------------------------------------------*/
