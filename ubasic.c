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

static int ended;

static value_t expr(void);
static void line_statement(void);
static void statement(void);
static void index_free(void);

peek_func peek_function = NULL;
poke_func poke_function = NULL;

line_t line_num;

static unsigned int array_base = 0;

/*---------------------------------------------------------------------------*/
void ubasic_init(const char *program)
{
  program_ptr = program;
  for_stack_ptr = gosub_stack_ptr = 0;
  index_free();
  tokenizer_init(program);
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
static uint8_t accept_tok(int token)
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
static int varfactor(void)
{
  int r;
  DEBUG_PRINTF("varfactor: obtaining %d from variable %d\n", variables[tokenizer_variable_num()], tokenizer_variable_num());
  r = ubasic_get_variable(tokenizer_variable_num());
  accept_tok(TOKENIZER_VARIABLE);
  return r;
}
/*---------------------------------------------------------------------------*/
static int factor(void)
{
  int r;
  uint8_t t = tokenizer_token();

  DEBUG_PRINTF("factor: token %d\n", tokenizer_token());
  switch(t) {
  case TOKENIZER_NUMBER:
    r = tokenizer_num();
    DEBUG_PRINTF("factor: number %d\n", r);
    accept_tok(TOKENIZER_NUMBER);
    break;
  case TOKENIZER_LEFTPAREN:
    accept_tok(TOKENIZER_LEFTPAREN);
    r = expr();
    accept_tok(TOKENIZER_RIGHTPAREN);
    break;
  case TOKENIZER_VARIABLE:
    r = varfactor();
    break;
  default:
    if (TOKENIZER_NUMEXP(t)) {
      accept_tok(t);
      accept_tok(TOKENIZER_LEFTPAREN);
      r = expr();
      switch(t) {
      case TOKENIZER_PEEK:
        r = peek_function(r);
        break;
      case TOKENIZER_ABS:
        if (r < 0)
          r = -r;
        break;
      case TOKENIZER_INT:
        break;
      case TOKENIZER_SGN:
        if (r > 1 ) r = 1;
        if (r < 0) r = -1;
        break;
      default:
        fprintf(stderr, "BOGOEXP\n");
      }
      accept_tok(TOKENIZER_RIGHTPAREN);
    }
    else {
      fprintf(stderr, "Line %d Syntax Error\n", line_num);
      exit(1);
    }
  }
  return r;
}
/*---------------------------------------------------------------------------*/
static int term(void)
{
  int f1, f2;
  int op;

  f1 = factor();
  op = tokenizer_token();
  DEBUG_PRINTF("term: token %d\n", op);
  while(op == TOKENIZER_ASTR ||
       op == TOKENIZER_SLASH ||
       op == TOKENIZER_MOD) {
    tokenizer_next();
    f2 = factor();
    DEBUG_PRINTF("term: %d %d %d\n", f1, op, f2);
    switch(op) {
    case TOKENIZER_ASTR:
      f1 = f1 * f2;
      break;
    case TOKENIZER_SLASH:
      f1 = f1 / f2;
      break;
    case TOKENIZER_MOD:
      f1 = f1 % f2;
      break;
    }
    op = tokenizer_token();
  }
  DEBUG_PRINTF("term: %d\n", f1);
  return f1;
}
/*---------------------------------------------------------------------------*/
static value_t expr(void)
{
  value_t t1, t2;
  int op;

  t1 = term();
  op = tokenizer_token();
  DEBUG_PRINTF("expr: token %d\n", op);
  while(op == TOKENIZER_PLUS ||
       op == TOKENIZER_MINUS ||
       op == TOKENIZER_AND ||
       op == TOKENIZER_OR) {
    tokenizer_next();
    t2 = term();
    DEBUG_PRINTF("expr: %d %d %d\n", t1, op, t2);
    switch(op) {
    case TOKENIZER_PLUS:
      t1 = t1 + t2;
      break;
    case TOKENIZER_MINUS:
      t1 = t1 - t2;
      break;
    case TOKENIZER_AND:
      t1 = t1 & t2;
      break;
    case TOKENIZER_OR:
      t1 = t1 | t2;
      break;
    }
    op = tokenizer_token();
  }
  DEBUG_PRINTF("expr: %d\n", t1);
  return t1;
}
/*---------------------------------------------------------------------------*/
static int relation(void)
{
  value_t r1, r2;
  int op;

  r1 = expr();
  op = tokenizer_token();
  DEBUG_PRINTF("relation: token %d\n", op);
  while(op == TOKENIZER_LT ||
       op == TOKENIZER_GT ||
       op == TOKENIZER_EQ) {
    tokenizer_next();
    r2 = expr();
    DEBUG_PRINTF("relation: %d %d %d\n", r1, op, r2);
    switch(op) {
    case TOKENIZER_LT:
      r1 = r1 < r2;
      break;
    case TOKENIZER_GT:
      r1 = r1 > r2;
      break;
    case TOKENIZER_EQ:
      r1 = r1 == r2;
      break;
    }
    op = tokenizer_token();
  }
  return r1;
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
static void goto_statement(void)
{
  accept_tok(TOKENIZER_GOTO);
  jump_linenum(expr());
}
/*---------------------------------------------------------------------------*/

static void charout(char c, void *unused)
{
  putchar(c);
}

static void print_statement(void)
{
  uint8_t nonl;
  accept_tok(TOKENIZER_PRINT);
  do {
    nonl = 0;
    DEBUG_PRINTF("Print loop\n");
    if(TOKENIZER_STRINGEXP(tokenizer_token())) {
      tokenizer_string_func(charout, NULL);
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_COMMA) {
      printf("\t");
      nonl = 1;
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_SEMICOLON) {
      nonl = 1;
      tokenizer_next();
    } else if(TOKENIZER_NUMEXP(tokenizer_token())) {
        printf("%d", expr());
    } else {
      printf("TOK type %d\n", tokenizer_token());
      break;
    }
  } while(tokenizer_token() != TOKENIZER_CR &&
      tokenizer_token() != TOKENIZER_ENDOFINPUT);
  if (!nonl)
    printf("\n");
  DEBUG_PRINTF("End of print\n");
  tokenizer_next();
}
/*---------------------------------------------------------------------------*/
static void if_statement(void)
{
  int r;

  accept_tok(TOKENIZER_IF);

  r = relation();
  DEBUG_PRINTF("if_statement: relation %d\n", r);
  accept_tok(TOKENIZER_THEN);
  if(r) {
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

  var = tokenizer_variable_num();

  accept_tok(TOKENIZER_VARIABLE);
  accept_tok(TOKENIZER_EQ);
  ubasic_set_variable(var, expr());
  DEBUG_PRINTF("let_statement: assign %d to %d\n", variables[var], var);
  accept_tok(TOKENIZER_CR);

}
/*---------------------------------------------------------------------------*/
static void gosub_statement(void)
{
  int linenum;
  accept_tok(TOKENIZER_GOSUB);
  linenum = expr();
  accept_tok(TOKENIZER_CR);
  if(gosub_stack_ptr < MAX_GOSUB_STACK_DEPTH) {
    gosub_stack[gosub_stack_ptr] = tokenizer_num();
    gosub_stack_ptr++;
    jump_linenum(linenum);
  } else {
    DEBUG_PRINTF("gosub_statement: gosub stack exhausted\n");
  }
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

  /* FIXME: support 'NEXT' on its own, also loop down the stack so if you
     GOTO out of a layer of NEXT the right thing occurs */
  accept_tok(TOKENIZER_NEXT);
  var = tokenizer_variable_num();
  accept_tok(TOKENIZER_VARIABLE);
  
  /* FIXME: make the for stack just use pointers so it compiles better */
  fs = &for_stack[for_stack_ptr - 1];
  if(for_stack_ptr > 0 &&
     var == fs->for_variable) {
    ubasic_set_variable(var,
                       ubasic_get_variable(var) + fs->step);
    /* NEXT end depends upon sign of STEP */
    if ((fs->step >= 0 && ubasic_get_variable(var) <= fs->to) ||
        (fs->step < 0 && ubasic_get_variable(var) >= fs->to)) {
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

  accept_tok(TOKENIZER_FOR);
  for_variable = tokenizer_variable_num();
  accept_tok(TOKENIZER_VARIABLE);
  accept_tok(TOKENIZER_EQ);
  ubasic_set_variable(for_variable, expr());
  accept_tok(TOKENIZER_TO);
  to = expr();
  if (tokenizer_token() == TOKENIZER_STEP) {
    accept_tok(TOKENIZER_STEP);
    step = expr();
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
  poke_addr = expr();
  accept_tok(TOKENIZER_COMMA);
  value = expr();
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
    t = tokenizer_token();
    if (t != TOKENIZER_CR && t != TOKENIZER_COMMA) {
      fprintf(stderr, "Line %d, syntax error\n", line_num);
      exit(1);
    }
    accept_tok(t);
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
    r = expr();
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
  r = expr();
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
  value_t r;
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

  /* Consider the single var allowed version of INPUT - it's saner for
     strings by far ? */
  do {  
    v = tokenizer_variable_num();
    accept_tok(TOKENIZER_VARIABLE);
  
    p = strtok(sp, " ,\t\n");
    sp = NULL;
    if (p == NULL)
      r = 0;
    else
      r = atoi(p);	/* FIXME: error checking */
    ubasic_set_variable(v, r);
    t = tokenizer_token();
    if (t != TOKENIZER_CR)
      accept_tok(TOKENIZER_COMMA);
  } while(t != TOKENIZER_CR);
  accept_tok(TOKENIZER_CR);
}


/*---------------------------------------------------------------------------*/
static void statement(void)
{
  int token;

  token = tokenizer_token();

  switch(token) {
  case TOKENIZER_PRINT:
    print_statement();
    break;
  case TOKENIZER_IF:
    if_statement();
    break;
  case TOKENIZER_GOTO:
    goto_statement();
    break;
  case TOKENIZER_GOSUB:
    gosub_statement();
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
  case TOKENIZER_LET:
    accept_tok(TOKENIZER_LET);
    /* Fall through. */
  case TOKENIZER_VARIABLE:
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
void ubasic_set_variable(int varnum, value_t value)
{
  if(varnum >= 0 && varnum <= MAX_VARNUM) {
    variables[varnum] = value;
  }
}
/*---------------------------------------------------------------------------*/
value_t ubasic_get_variable(int varnum)
{
  if(varnum >= 0 && varnum <= MAX_VARNUM) {
    return variables[varnum];
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
