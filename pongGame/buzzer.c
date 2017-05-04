#include <msp430.h>
// .arch msp430
#include "buzzer.h"

static unsigned int period = 1000;
static signed int rate = 200;

#define MIN_PERIOD 1000
#define MAX_PERIOD 4000

#define PORT2_CONFIGURE P2SEL2
#define P2_PIN_BEHAVIOR P2SEL
#define P2_INPUT_OUTPUT P2DIR



void buzzer_init() {

    /*
       Direct timer A output "TA0.1" to P2.6.
        According to table 21 from data sheet:
          P2SEL2.6, P2SEL2.7, anmd P2SEL.7 must be zero
          P2SEL.6 must be 1
        Also: P2.6 direction must be output
    */
    timerAUpmode();        /* used to drive speaker */
    P2SEL2 &= ~(BIT6 | BIT7);
    P2SEL &= ~BIT7;
    P2SEL |= BIT6;
    P2DIR = BIT6;        /* enable output to speaker (P2.6) */

}
    void upBuzz() {
        if ((rate > 0 && (period > MAX_PERIOD)) ||
            (rate < 0 && (period < MIN_PERIOD))) {
            rate = -rate;
            period += (rate << 1);
        }
        buzzer_set_period(period ^ 1000);
    }

    void downBuzz() {
        if ((rate > 0 && (period > MAX_PERIOD)) ||
            (rate < 0 && (period < MIN_PERIOD))) {
            rate = -rate;
            period += (rate << 1);
        }
        buzzer_set_period(period ^ 1000);
    }

    void hitBuzz() {
        if ((rate > 0 && (period > MAX_PERIOD)) ||
            (rate < 0 && (period < MIN_PERIOD))) {
            rate = -rate;
            period += (rate << 1);
        }
        buzzer_set_period(period ^ 1000);
    }

    void loseBuzz() {
        if ((rate > 0 && (period > MAX_PERIOD)) ||
            (rate < 0 && (period < MIN_PERIOD))) {
            rate = -rate;
            period += (rate << 1);
        }
        buzzer_set_period(period ^ 1000);
    }

/*
buzzer_set_period: mov r12, &CCR0
            mov &CCRO, r14
            rrc r14
            mov r14 &CCR1
            RET
*/
void buzzer_set_period(short cycles) {
    CCR0 = cycles -1000;
    CCR1 = cycles >> 1;		/* one half cycle */
}
