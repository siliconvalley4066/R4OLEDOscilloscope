//unsigned long samplingTime = 0;

int16_t analogReadf(byte ad_ch) {
  uint16_t adc_value;
  if (ad_ch == A1) {
    R_ADC0->ADANSA_b[0].ANSA0 = 1;  // A1 port target
    R_ADC0->ADANSA_b[0].ANSA1 = 0;  // A2 port no access
    R_ADC0->ADCSR_b.ADST = 1;       // start conversion
    while (R_ADC0->ADCSR_b.ADST);   // wait complete flag
    adc_value = R_ADC0->ADDR[0];    // get the 14 bits ADC value
  } else if (ad_ch == A2) {
    R_ADC0->ADANSA_b[0].ANSA0 = 0;  // A1 port no access
    R_ADC0->ADANSA_b[0].ANSA1 = 1;  // A2 port target
    R_ADC0->ADCSR_b.ADST = 1;       // start conversion
    while (R_ADC0->ADCSR_b.ADST);   // wait complete flag
    adc_value = R_ADC0->ADDR[1];    // get the 14 bits ADC value
  } else {
    adc_value = 0;
  }
  return adc_value >> 2;
}

void sample_dual_usf(unsigned int r) {  // dual channel
  R_ADC0->ADANSA_b[0].ANSA0 = 1;    // A1 port target
  R_ADC0->ADANSA_b[0].ANSA1 = 1;    // A2 port target
  //  samplingTime = micros();
  unsigned long st = micros();
  for (int i = 0; i < SAMPLES; i ++) {
    while (micros() - st < r) ;
    R_ADC0->ADCSR_b.ADST = 1;       // start conversion
    while (R_ADC0->ADCSR_b.ADST);   // wait complete flag
    cap_buf[i]  = R_ADC0->ADDR[0] >> 2;
    cap_buf1[i] = R_ADC0->ADDR[1] >> 2;
    st += r;
  }
  //  samplingTime = micros() - samplingTime;
  scaleDataArray(ad_ch0, 0);
  scaleDataArray(ad_ch1, 0);
}

void sample_usf(void) { // single channel full speed
  byte ch, ad;
  uint16_t *p;
  if (ch0_mode == MODE_OFF && ch1_mode != MODE_OFF) {
    R_ADC0->ADANSA_b[0].ANSA0 = 0;  // A1 port no access
    R_ADC0->ADANSA_b[0].ANSA1 = 1;  // A2 port target
    ch = ad_ch1;
    p = cap_buf1;
    ad = 1;
  } else {
    R_ADC0->ADANSA_b[0].ANSA0 = 1;  // A1 port target
    R_ADC0->ADANSA_b[0].ANSA1 = 0;  // A2 port no access
    ch = ad_ch0;
    p = cap_buf;
    ad = 0;
  }
  //  samplingTime = micros();
  for (int i = 0; i < NSAMP / 2; i ++) {
    R_ADC0->ADCSR_b.ADST = 1;       // start conversion
    while (R_ADC0->ADCSR_b.ADST);   // wait complete flag
    p[i] = R_ADC0->ADDR[ad];        // get the 14 bits ADC value
  }
  //  samplingTime = micros() - samplingTime;
  for (int i = 0; i < NSAMP / 2; i ++) {
    p[i] = p[i] >> 2;
  }
  int t = trigger_point();
  scaleDataArray(ch, t);
}

//void debug_print() {
//  display.setTextColor(TXTCOLOR, BGCOLOR);
//  display.setCursor(12, 35); display.print("      ");
//  display.setCursor(12, 35); display.print(samplingTime);
//}

int trigger_point() {
  int trigger_ad, i;
  uint16_t *cap;

  if (trig_ch == ad_ch1) {
    trigger_ad = advalue(trig_lv, VREF[range1], ch1_mode, ch1_off);
    cap = cap_buf1;
  } else {
    trigger_ad = advalue(trig_lv, VREF[range0], ch0_mode, ch0_off);
    cap = cap_buf;
  }
  for (i = 0; i < (NSAMP / 2 - SAMPLES); ++i) {
    if (trig_edge == TRIG_E_UP) {
      if (cap[i] < trigger_ad && cap[i + 1] > trigger_ad)
        break;
    } else {
      if (cap[i] > trigger_ad && cap[i + 1] < trigger_ad)
        break;
    }
  }
  return i;
}
