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
  clock_t start_t, end_t;
  double delta_t;

  start_t = clock();

  ubasic_init_peek_poke(program, &peek, &poke);

  do {
    ubasic_run();
  } while(!ubasic_finished());

  end_t = clock();
  delta_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;

  printf("done. Run time: %.3f s\n", delta_t);
}

/*---------------------------------------------------------------------------*/

static char buf[16384];	/* eww */

int main(int argc, char *argv[])
{
  int fd, l;

  if (argc != 2) {
    fprintf(stderr, "%s: program\n", argv[0]);
    exit(1);
  }
  fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror(argv[1]);
    exit(1);
  }
  l = read(fd, buf, 16384);
  if (l == 16384) {
    fprintf(stderr, "%s is too long\n", argv[1]);
    exit(1);
  }
  close(fd);
  printf("Loaded %d bytes\n", l);
  buf[l] = 0;
  run(buf);
  return 0;
}
