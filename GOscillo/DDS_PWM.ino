/*
 * DDS Sine Generator mit ATMEGS 168
 * Timer generates the 30.0 kHz Clock Interrupt
 *
 * KHM 2009 /  Martin Nawrath
 * Kunsthochschule fuer Medien Koeln
 * Academy of Media Arts Cologne
 */

#include "FspTimer.h"

FspTimer dtimer;

extern const unsigned char sine256[], saw256[], revsaw256[], triangle[], rect256[];
extern const unsigned char pulse20[], pulse10[], pulse05[], delta[], noise[];
extern const unsigned char gaussian_noise[], ecg[], sinc5[], sinc10[], sinc20[];
extern const unsigned char sine2harmonic[], sine3harmonic[], choppedsine[];
extern const unsigned char sinabs[], trapezoid[], step2[], step4[], chainsaw[];
unsigned char *wp;
const unsigned char * wavetable[] PROGMEM = {sine256, saw256, revsaw256, triangle, rect256,
  pulse20, pulse10, pulse05, delta, noise, gaussian_noise, ecg, sinc5, sinc10, sinc20,
  sine2harmonic, sine3harmonic, choppedsine, sinabs, trapezoid, step2, step4, chainsaw};
const char Wavename[][5] PROGMEM = {"Sine", "Saw", "RSaw", "Tri", "Rect",
  "PL20", "PL10", "PL05", "Dlta", "Nois", "GNoi", "ECG", "Snc1", "Snc2", "Snc3",
  "Sin2", "Sin3", "CSin", "Sabs", "Trpz", "Stp2", "Stp4", "Csaw"};
const byte wave_num = (sizeof(wavetable) / sizeof(&sine256));
long ifreq = 50000; // frequency * 100 for 0.01Hz resolution
byte wave_id = 0;
#define DDSPin A0

// 12-Bit D/A Converter
#define DACBASE 0x40050000  // DAC Base - DAC output on A0 (P014 AN09 DAC)
#define DAC12_DADR0 ((volatile unsigned short *)(DACBASE + 0xE000)) // D/A Data Register 0 

double refclk=30000.0f;           // System clock is 48MHz

// variables used inside interrupt service declared as voilatile
volatile byte icnt;             // var inside interrupt
volatile unsigned long phaccu;  // pahse accumulator
volatile unsigned long tword_m; // dds tuning word m

//#define ANALOGWAVE
#ifndef ANALOGWAVE
void dds_setup() {
  analogWriteResolution(8);  // set the analog output resolution to 8 bit (256 levels)
  analogWrite(DAC, 0);
//  refclk = sys_clk / 1600.0f;
  Setup_timer();
  tword_m=pow(2,32)*ifreq*0.01/refclk; // calulate DDS new tuning word
  wp = (unsigned char *) wavetable[wave_id];
}

void dds_close() {
  dtimer.stop();
  dtimer.end();
  pinMode(DDSPin, INPUT);
}

void dds_set_freq() {
  double dfreq;
  dfreq = (double)ifreq*0.01;     // adjust output frequency
  tword_m=pow(2,32)*dfreq/refclk; // calulate DDS new tuning word
}
#else
#include "analogWave.h"
analogWave wave(DAC);
void dds_setup() {
  analogWriteResolution(8);  // set the analog output resolution to 8 bit (256 levels)
  wave.sine((float)ifreq * 0.01);
}

void dds_close() {
  wave.stop();
  pinMode(DDSPin, INPUT);
}

void dds_set_freq() {
  wave.freq((float)ifreq * 0.01);
}
#endif

void rotate_wave(bool fwd) {
  if (fwd) {
    wave_id = (wave_id + 1) % wave_num;
  } else {
    if (wave_id > 0) --wave_id;
    else wave_id = wave_num - 1;
  }
  wp = (unsigned char *) wavetable[wave_id];
}

void set_wave(int id) {
  wave_id = id;
  wp = (unsigned char *) wavetable[wave_id];
}

//******************************************************************
// Timer setup
// set 30.0kHz clock
void Setup_timer() {
  uint8_t type;
  int8_t ch = FspTimer::get_available_timer(type);
  if (ch < 0) {
    return;
  }
  dtimer.begin(TIMER_MODE_PERIODIC, type, ch, (float)refclk, 50.0f, callbackfunc, nullptr);
  dtimer.setup_overflow_irq();
  dtimer.open();
  dtimer.start();
}

//******************************************************************
// Timer Interrupt Service at 30kHz = 33.3uSec
// this is the timebase REFCLOCK for the DDS generator
// FOUT = (M (REFCLK)) / (2 exp 32)
// runtime : ? microseconds ( inclusive push and pop)
void callbackfunc(timer_callback_args_t *arg) {
  phaccu=phaccu+tword_m;  // soft DDS, phase accu with 32 bits
  icnt=phaccu >> 24;      // use upper 8 bits for phase accu as frequency information
                          // read value fron ROM sine table and send to PWM DAC
//  analogWrite(DAC, wp[icnt]);
  *DAC12_DADR0 = wp[icnt]<<4;  // DAC update - takes 210nS - DAC ignores top 4 bits
}

void update_ifrq(long diff) {
  long newFreq;
  int fast;
  if (diff != 0) {
    if (abs(diff) > 3) {
      fast = ifreq / 40;
    } else if (abs(diff) > 2) {
      fast = ifreq / 300;
    } else if (abs(diff) > 1) {
      fast = 25;
    } else {
      fast = 1;
    }
    if (fast < 1) fast = 1;
    newFreq = ifreq + fast * diff;
  } else {
    newFreq = ifreq;
  }
  newFreq = constrain(newFreq, 1, 999999);
  if (newFreq != ifreq) {
    ifreq = newFreq;
    dds_set_freq();
  }
}

float set_freq(float dfreq) {
  long newfreq = 100.0 * dfreq;
  ifreq = constrain(newfreq, 1, 999999);
  dds_set_freq();
  return (dds_freq());
}

float dds_freq(void) {
  return (float)ifreq * 0.01;
}

void disp_dds_freq(void) {
  display.print(dds_freq(), 2); display.print("Hz");
}

void disp_dds_wave(void) {
  display.print(Wavename[wave_id]); 
}
