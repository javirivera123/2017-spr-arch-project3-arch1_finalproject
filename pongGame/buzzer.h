#ifndef buzzer_included
#define buzzer_included

void buzzer_init();
void start_button_sound();
void buzzer_set_period(short cycles);
void upBuzz();
void downBuzz();
void hitBuzz();
void loseBuzz();

#endif // included
