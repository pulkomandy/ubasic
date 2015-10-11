all: tests use-ubasic ubx

CFLAGS=-Wall -pedantic -g3

tests: tests.o ubasic.o tokenizer.o
use-ubasic: use-ubasic.o ubasic.o tokenizer.o
use-ubasic: use-ubasic.o ubasic.o tokenizer.o
ubx: ubx.o ubasic.o tokenizer.o
clean:
	rm -f *.o tests use-ubasic ubx *~

ubx.c: ubasic.h
tests.c: ubasic.h
use-ubasic.c: ubasic.h
ubasic.c: ubasic.h tokenizer.h
tokenizer.c: ubasic.h tokenizer.h
