/*
   Arduino UNO R4 Frequency Counter Library Version 1.01
   Basic idea come from http://igarage.cocolog-nifty.com/blog/2025/05/post-ab016e.html
   Copyright (c) 2025, Siliconvalley4066
   Licenced under the GNU GPL Version 3.0
*/
#include "FreqCount_R4.h"

FspTimer ftimer;

volatile bool FreqCountClass::f_cap0;       // first capture
volatile uint8_t FreqCountClass::f_frq;     // measure valid flag
volatile uint32_t FreqCountClass::cnt_0;    // previous capture value
volatile uint32_t FreqCountClass::cnt_1;    // next capture value
volatile uint32_t FreqCountClass::cnt_frq;  // count value
uint32_t FreqCountClass::gtpr;              // gate time count
uint32_t FreqCountClass::pre;               // gate time prescaler

void FreqCountClass::begin(uint16_t msec) {
  Setup_timer();
  gtpr_pre((uint32_t) msec);
  init();
  f_cap0 = true;
  f_frq = 0;
}

void FreqCountClass::init(void) {
//  PFS protect off
  R_PMISC->PWPR_b.B0WI      = 0;    // PFS write protect off
  R_PMISC->PWPR_b.PFSWE     = 1;
  R_MSTP->MSTPCRD_b.MSTPD5  = 0;    // 32bit PWM module start
  R_MSTP->MSTPCRD_b.MSTPD6  = 0;    // 16bit PWM module start
  R_MSTP->MSTPCRC_b.MSTPC14 = 0;    // ELC module start
// GPT6 B out 1Hz  capture at falling edge
  R_GPT6->GTCR_b.CST    = 0;        // stop count
  if (pre == 64) {
    R_GPT6->GTCR = 0b011 << 24;     // mode 0b000, PCLKD/64 750000Hz
  } else if (pre == 256) {
    R_GPT6->GTCR = 0b100 << 24;     // mode 0b000, PCLKD/256 187500Hz
  } else {
    R_GPT6->GTCR = 0b101 << 24;     // mode 0b000, PCLKD/1024 46875Hz
  }
  R_GPT6->GTPR        = gtpr - 1;   // 1Hz
  R_GPT6->GTPBR       = gtpr - 1;   // 1Hz
  R_GPT6->GTCCR[1]    = gtpr / 2;   // GTIOCB H count
  R_GPT6->GTIOR_b.GTIOB = 0b00110;  // GTIOC2B L at end period, B H at match
  R_GPT6->GTIOR_b.OBE   = 0;        // B disable output
  R_GPT6->GTCNT         = 0;        // clear count
  R_GPT6->GTCR_b.CST    = 1;        // start count
//  GPT1(32bit) capture by ELC
  R_GPT1->GTICBSR_b.BSELCA            = 1;        // ELC_GPTA B capture
// D2 P105 GPT1A clock input up count
//  R_PFS->PORT[1].PIN[5].PmnPFS_b.PCR  = 1;        // P105 input pullup
  R_PFS->PORT[1].PIN[5].PmnPFS_b.PSEL = 0b00011;  // GTIOC1A input
  R_PFS->PORT[1].PIN[5].PmnPFS_b.PMR  = 1;        // activate peripheral
  R_GPT1->GTUPSR_b.USCAFBL            = 1;        // B=L:count up at falling edge of A
  R_GPT1->GTUPSR_b.USCAFBH            = 1;        // B=H:count up at falling edge of A
  R_GPT1->GTCR_b.CST                  = 1;        // start count
  //  ELC GPT6 overflow generates ELC_GPTA
  R_ELC->ELSR[0].HA   = ELC_EVENT_GPT6_COUNTER_OVERFLOW;  // GPT6_OVF
  R_ELC->ELCR_b.ELCON = 1;        // enable ELC
}

uint8_t FreqCountClass::available(void) {
  return f_frq;
}

uint32_t FreqCountClass::read(void) {
  noInterrupts();
  uint32_t count = cnt_frq;
  interrupts();
  f_frq = 0;
  return count;
}

void FreqCountClass::end(void) {
  ftimer.stop();
  ftimer.end();
}

void FreqCountClass::gatetime(uint16_t msec) {
  gtpr_pre((uint32_t) msec);
  init();
  f_cap0 = true;
}

/*
   freq = 48000000 / gtpr / pre = 1000 / millis
   48000 * millis = gtpr * pre
   Acceptable ms are 1...88,100,200,400,600,800,1000 and some otheres.
   Other value cause some errors.
*/

bool FreqCountClass::gtpr_pre(uint32_t ms) {
  bool ok = true;
  if (ms < 1) {
    ms = 1; ok = false;
  } else if (ms > 1000) {
    ms = 1000; ok = false;
  }
  pre = 64;
  gtpr = (48000 * ms) / pre;
  if (gtpr > 65535) pre = 256;
  gtpr = (48000 * ms) / pre;
  if (gtpr > 65535) pre = 1024;
  gtpr = (48000 * ms) / pre;
  if ((48000 * ms) != (pre * gtpr)) ok = false;
  return ok;
}

//******************************************************************
// Timer Interrupt Service
void FreqCountClass::freqcount_isr(timer_callback_args_t __attribute((unused)) *arg) {
  if (R_GPT1->GTST_b.TCFB != 0) { // capture occured
    R_GPT1->GTST_b.TCFB = 0;      // clear capture flag
    cnt_1 = R_GPT1->GTCCR[1];     // get CAP B data
    if (f_cap0) {                 // is first capture?
      cnt_0 = cnt_1;              // initialize cap0
      f_cap0 = false;
    } else {
      cnt_frq = cnt_1 - cnt_0;    // difference to previous capture
      cnt_0 = cnt_1;              // update cap0
      f_frq = 1;                  // measure is valid
    }
  }
}

void FreqCountClass::Setup_timer(void) {
  uint8_t type = 0;
  int8_t ch = 6;  // GPT6
  ftimer.begin(TIMER_MODE_PERIODIC, type, ch, 1.0f, 50.0f, freqcount_isr, nullptr);
  ftimer.setup_overflow_irq();
  ftimer.open();
}

FreqCountClass FreqCount;
