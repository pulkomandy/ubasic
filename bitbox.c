#include <stdint.h>
#include <unistd.h>
#include <setjmp.h>

#include "ubasic.h"
#include "lib/textmode/textmode.h"
#include "lib/events/events.h"

extern jmp_buf exception;

void clear_display(void)
{
	clear();

	set_palette(0, RGB(255, 255, 128), RGB(128,  0,128));
	set_palette(1, RGB(128,   0, 128), RGB(255,255,128)); // inverse video for cursor
}

int move_cursor(int x, int y)
{
  return 0;
}


size_t _read(int stream, char* data, int size)
{
	struct event e;
	int p = 0;

next:
	do {
		wait_vsync(1);
		events_poll();
		e = event_get();
	} while (e.type != evt_keyboard_press || e.kbd.sym < 8);

	data[p++] = e.kbd.sym;
	charout(e.kbd.sym);
    if (e.kbd.sym == 8)
        p -= 2;

	if (p == size)
		return p;
	if (e.kbd.sym == '\n') {
		data[p + 1] =0;
		return p;
	}
	goto next;
}

int error;
extern void statements(void);
static char program[65536];

/*---------------------------------------------------------------------------*/
void
bitbox_main(void)
{
  char* buf = program;
  clear_display();

  putstrz("BITBOX BASIC v1.0\n(c) 2006, Adam Dunkels; 2015-2016, Alan Cox; "
    "2017, Adrien Destugues\n");
  putstrz("\nREADY\n");

  setjmp(exception);
  for(;;) {
    error = 0;
    begin_input();
    int len = _read(0, buf, 256);
    end_input();

    if (isdigit(buf[0]))
    {
        // This looks like it starts with a line number. Ideally we should
        // insert it in the right position in the line list and possibly
        // overwrite another line with the same number. For now we just append
        // it to the program buffer, however.
        buf += len;
    } else if (strncmp(buf, "new", 3) == 0) {
        program[0] = 0;
        putstrz("\nREADY\n");
    } else if (strncmp(buf, "run", 3) == 0) {
        buf[0] = 0;
        // Now run the program
        ubasic_init(program);
        do {
          ubasic_run();
        } while(!ubasic_finished() && error == 0);
        putstrz("\nREADY\n");
    } else if (strncmp(buf, "list", 4) == 0) {
        buf[0] = 0;
        putstrz(program);
        putstrz("\nREADY\n");
    } else {
        // Direct command, execute it in interactive mode
        // NOTE: maybe ubasic_init is a it brutal here, usually we don't want
        // to kill the whole program context (variables, etc). Maybe reset just
        // the tokenizer.
        ubasic_init(buf);
        line_num = 0;
        statements();
        putstrz("\nREADY\n");
    }
  }
}

/*---------------------------------------------------------------------------*/


#ifndef EMULATOR
// This is needed for use of sprintf
void* __attribute__((used)) _sbrk(intptr_t increment)
{
	extern void* end;
	static void* ptr = &end;

	ptr += increment;
	return ptr;
}

void _exit(int unused)
{
	// FIXME jump back to interpreter or something?
	for(;;);
}

void _write(int stream, char* data, int size)
{
}

void _close()
{
}

void _lseek()
{
}
#endif
