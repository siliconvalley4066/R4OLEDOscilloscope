#ifndef FreqCountR4_h
#define FreqCountR4_h

#include <Arduino.h>
#include "FspTimer.h"

class FreqCountClass {
  public:
    static void begin(uint16_t msec);
    static uint8_t available(void);
    static uint32_t read(void);
    static void end(void);
    static void gatetime(uint16_t msec);

  private:
    static void Setup_timer(void);
    static void init(void);
    static void freqcount_isr(timer_callback_args_t *arg);
    static bool gtpr_pre(uint32_t ms);
    static volatile bool f_cap0;
    static volatile uint32_t cnt_0;
    static volatile uint32_t cnt_1;
    static volatile uint32_t cnt_frq;
    static volatile uint8_t f_frq;
    static uint32_t gtpr;
    static uint32_t pre;
};

extern FreqCountClass FreqCount;

#endif
