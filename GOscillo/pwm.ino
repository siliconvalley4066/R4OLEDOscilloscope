#include "pwm.h"

#define PWMPin 10
PwmOut pwm(PWMPin);

byte duty = 128;  // duty ratio = duty/256
byte p_range = 0;
unsigned short count;
const long range_min[1] PROGMEM = {1};
const long range_div[1] PROGMEM = {1};

double pulse_frq(void) {          // 31Hz <= freq <= 1MHz
  long divide = range_div[p_range];
  return(2000000.0d / ((double)((long)count + 1) * (double)divide));
}

void pulse_init() {
  long divide;
  p_range = 0;  // constrain(p_range, 0, 16);
  divide = range_div[p_range];
  float fduty = duty*100.0/256.0;
  pwm.begin((float)pulse_frq(), fduty);
}

void update_frq(int diff) {
  int fast;
  long newCount;

  if (abs(diff) > 3) {
    fast = 512;
  } else if (abs(diff) > 2) {
    fast = 128;
  } else if (abs(diff) > 1) {
    fast = 25;
  } else {
    fast = 1;
  }
  newCount = (long)count + fast * diff;

  if (newCount < range_min[p_range]) {
    if (p_range < 1) {
      newCount = 1;
    } else {
      --p_range;
      newCount = 65535;
    }
  } else if (newCount > 65535) {
    if (p_range < 0) {
      ++p_range;
      newCount = range_min[p_range];
    } else {
      newCount = 65535;
    }
  }
  count = newCount;
  float fduty = duty*100.0/256.0;
  pwm.end();
  pwm.begin((float)pulse_frq(), fduty);
}

void disp_pulse_frq(void) {
  float freq = pulse_frq();
  if (freq < 10.0) {
    display.print(freq, 5);
  } else if (freq < 100.0) {
    display.print(freq, 4);
  } else if (freq < 1000.0) {
    display.print(freq, 3);
  } else if (freq < 10000.0) {
    display.print(freq, 2);
  } else if (freq < 100000.0) {
    display.print(freq, 1);
  } else if (freq < 1000000.0) {
    display.print(freq * 1e-3, 2); display.print('k');
  } else if (freq < 10000000.0) {
    display.print(freq * 1e-6, 4); display.print('M');
  } else {
    display.print(freq * 1e-6, 3); display.print('M');
  }
  display.print("Hz");
}

void disp_pulse_dty(void) {
  static bool sp = true;
  float fduty = duty*100.0/256.0;
  display.print(fduty, 1); display.print('%');
  if (fduty < 9.95) {
    if (sp) {
      display.print(' ');
      sp = false;
    }
  } else {
    sp = true;
  }
}

void pulse_start(void) {
  pulse_init();
}

void pulse_close(void) {
  pwm.end();
  pinMode(PWMPin, INPUT);
}
