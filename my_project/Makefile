# makfile configuration
CPU             	= msp430g2553
CFLAGS          	= -mmcu=${CPU} -Os -I../h -I../h -I../p2swLib

#switch the compiler (for the internal make rules)
CC              = msp430-elf-gcc
AS              = msp430-elf-gcc -mmcu=${CPU} -c

all:project3.elf

#additional rules for files
project3.elf: ${COMMON_OBJECTS} project3.o wdt_handler.o music.o buzzer.o
	${CC} -mmcu=${CPU}   -o $@ $^ -L../lib -lTimer -lLcd -lShape -lCircle -lp2sw

load: project3.elf
	mspdebug rf2500 "prog $^"

clean:
	rm -f *.o *.elf
