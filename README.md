This is a fork of ubasic with the aims of

- Making it smaller (removing lots of the more memory expensive coding)
- Bringing it up to ECMA55 and a bit beyond
- Making it faster (pretokenisation etc)
- Making it run on FUZIX boxen usefully

Currently

- Negative numbers are now supported
- Types for things are a bit clearer as they will matter
- Upper or lower case is permitted
- Errors in the original debugging code are fixed
- Variables may now be A-Z or A0-A9..Z0-Z9 as per ECMA55
- PRINT supports , for tab fields, ; for supressing newline
- PRINT constant string length is unlimited
- INPUT is added including support for a prompt
- FOR NEXT now supports STEP as per ECMA55
- PEEK() is now a function as in normal basic - X = PEEK(4)
- STOP replaces the rather odd "END"
- ABS() INT() and SGN() are implemented
- REM works as in normal BASIC
- DATA is supported but not yet RESTORE/READ !
- AND and OR keywords work (but not yet NOT)
- GOTO and GOSUB allow expressions "computed GOTO")

In comparison with ECMA55, then apart from all the floaty stuff it's missing

- Arrays (1 or 2 dimensions required by ECMA55)
- String variables (A$-Z$ required)
- The ^ operator (or ** equivalent)
- RND
- SQR
- DEF FN / FN (single or no variable required by ECMA55)
- PRINT TAB() (SPC() is not ECMA55 nor AT)
- READ
- RESTORE
- Unquoted data strings
- GO SUB and GO TO are two tokens so can be spaced

Other useful stuff to add

- XOR
- LEFT$(), RIGHT$(), MID$(), CHR$()
- VAL(),CODE()
- USR()
- LEN()
- INKEY$
- CLS
- ON expr GO TO/SUB  ... {ELSE}
- ON ERROR
- ON TIMER/SIGNAL
- PAUSE
- Use of ":"
- Short tokens "P." etc
- DO WHILE
- DO UNTIL
- I/O streams PRINT# INPUT# OPEN# CLOSE# etc
- Command mode
- CLEAR
- NEW
- Unix syscall bindings 8)
