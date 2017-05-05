.file buzzer.s
.text        ; Assemble into program memory

#include <msp430.h>
#include "buzzer.h"
#include <libTimer.h>

.extern P2SEL2
.extern P2SEL
.extern P2DIR
.extern BIT6
.extern BIT7
.extern CCR0
.extern CCR1
period: .word 1000
MAX: .word 4000
MIN: .word 1000


buzzer_init: call #timerAUpMode()
move &BIT6, r12
bis  &BIT7, r13
biz r12, r13
and r13, &P2SEL
bic &BIT7, P2SEL
bis &BIT6, &P2SEL
move &BIT6, &P2DIR
RET

buzzer_set_period:
mov r12, &CCR1
add &CCR1, &CCR0
sub #100, &CCR0
shr #1, &CCR1
RET

upBuzz:
add #100, &period
move 0(r1), r12
call #buzzer_set_period()






