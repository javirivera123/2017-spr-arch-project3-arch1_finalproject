# makfile configuration
CPU             	= msp430g2553
CFLAGS          	= -mmcu=${CPU} -Os -I../h -I../h

#switch the compiler (for the internal make rules)
CC              = msp430-elf-gcc
AS              = msp430-elf-gcc -mmcu=${CPU} -c

all:ship_shapemotion.elf

#additional rules for files
ship_shapemotion.elf: ${COMMON_OBJECTS} ship_shapemotion.o ship_wdt_handler.o
	ship_buzzer.o
	${CC} -mmcu=${CPU}   -o $@ $^ -L../lib -lTimer -lLcd -lShape -lCircle -lp2sw

load: ship_shapemotion.elf
	mspdebug rf2500 "prog $^"

clean:
	rm -f *.o *.elf