/*
 * Copyright (c) 2006, Adam Dunkels
 * Copyright (c) 2013, Danyil Bohdan
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

#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termcap.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "ubasic.h"

/*---------------------------------------------------------------------------*/
value_t peek(value_t arg) {
    return arg;
}

/*---------------------------------------------------------------------------*/
void poke(value_t arg, value_t value) {
    assert(arg == value);
}

/*---------------------------------------------------------------------------*/
void run(const char program[]) {
  ubasic_init_peek_poke(program, &peek, &poke);

  do {
    ubasic_run();
  } while(!ubasic_finished());
}

/*---------------------------------------------------------------------------*/

#ifdef VISUAL

static char *cl, *cm;
static int rows, cols;
static int tsave;
static struct termios saved_termios;

void ttyfix(void)
{
  ioctl(0, TCSETS, &saved_termios);
}

void handler(int sig)
{
  if (tsave)
    ttyfix();
  kill(getpid(), sig);
  _exit(1);
}

static void visual_init(void)
{
  int p[2];
  int s;
  pid_t pid;
  struct winsize w;
  struct termios tw;

  if (ioctl(0, TIOCGWINSZ, &w)) {
    perror("tiocgwinsz");
    exit(1);
  }
  rows = w.ws_row;
  cols = w.ws_col;

  if (pipe(p)) {
    perror("pipe");
    exit(1);
  }
  pid = fork();
  switch(pid) {
    case -1:
      perror("fork");
      exit(1);
    case 0:
      close(p[0]);
      dup2(p[1],0);
      dup2(0,1);
      execl("/usr/lib/tchelp", "tchelp", "cl$cm$", NULL);
      perror("exec");
      _exit(1);
  }
  close(p[1]);
  wait(NULL);
  if (read(p[0], &s, sizeof(int)) != sizeof(int)) {
    perror("tchelp");
    exit(1);
  }
  cl = sbrk((s + 3) & ~3);
  if (cl == (void *)-1) {
    perror("sbrk");
    exit(1);
  }
  if (read(p[0], cl, s) != s) {
    perror("tchelp");
    exit(1);
  }
  close(p[0]);
  cm = cl + strlen(cl) + 1;
  if (ioctl(0, TCGETS, &saved_termios)) {
    perror("tcgets");
    exit(1);
  }

  signal(SIGINT, handler);
  signal(SIGQUIT, handler);

  tsave = 1;
  atexit(ttyfix);
  memcpy(&tw, &saved_termios, sizeof(struct termios));
  tw.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|ISIG);
  if (ioctl(0, TCSETS, &tw)) {
    perror("tcsets");
    exit(1);
  }
}

static int outit(int c)
{
  char x = c;
  write(1, &x, 1);
  return 0;
}

void clear_display(void)
{
  tputs(cl, rows, outit);
}

int move_cursor(int x, int y)
{
  if (*cm == 0)
    return 0;
  tputs(tgoto(cm, y, x), 2, outit);
  return 1;
}
#else

void visual_init(void)
{
}


void clear_display(void)
{
  write(1, "\012", 1);
}

int move_cursor(int x, int y)
{
  return 0;
}

#endif

static char *buf;

int main(int argc, char *argv[])
{
  int fd, l;
  struct stat s;

  if (argc != 2) {
    write(2, argv[0], strlen(argv[0]));
    write(2, ": program\n", 10);
    exit(1);
  }

  visual_init();

  fd = open(argv[1], O_RDONLY);
  if (fd == -1 || fstat(fd, &s) == -1) {
    perror(argv[1]);
    exit(1);
  }
  /* Align to the next quad */
  buf = sbrk((s.st_size|3) + 1);
  if (buf == (char *)-1) {
    write(2, "Out of memory.\n",15);
    exit(1);
  }
  l = read(fd, buf, s.st_size);
  if (l != s.st_size) {
    perror(argv[1]);
    exit(1);
  }
  close(fd);
  buf[l] = 0;
  run(buf);
  return 0;
}
