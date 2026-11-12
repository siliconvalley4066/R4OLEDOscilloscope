#include <FspTimer.h>
#include "hal_data.h"
#include "r_adc.h"
#include "r_dmac.h"
#include "r_elc.h"

adc_instance_ctrl_t adc_ctrl;
elc_instance_ctrl_t elc_ctrl;
static transfer_info_t dmacInfo;
static transfer_cfg_t dmacCfg;
static dmac_extended_cfg_t dmacExt;
static dmac_instance_ctrl_t dmacCtrl;
static transfer_info_t dmacInfo1;
static transfer_cfg_t dmacCfg1;
static dmac_extended_cfg_t dmacExt1;
static dmac_instance_ctrl_t dmacCtrl1;
static bool adc_timerdma = false;

FspTimer adcTimer;

int16_t analogReadf(byte ad_ch) {
  uint16_t adc_value;
  if (ad_ch == A1) {
    R_ADC0->ADANSA_b[0].ANSA0 = 1;  // A1 port target
    R_ADC0->ADANSA_b[0].ANSA1 = 0;  // A2 port no access
    R_ADC0->ADCSR_b.ADST = 1;       // start conversion
    while (R_ADC0->ADCSR_b.ADST);   // wait complete flag
    adc_value = R_ADC0->ADDR[0];  // get the 14 bits ADC value
  } else if (ad_ch == A2) {
    R_ADC0->ADANSA_b[0].ANSA0 = 0;  // A1 port no access
    R_ADC0->ADANSA_b[0].ANSA1 = 1;  // A2 port target
    R_ADC0->ADCSR_b.ADST = 1;       // start conversion
    while (R_ADC0->ADCSR_b.ADST);   // wait complete flag
    adc_value = R_ADC0->ADDR[1];  // get the 14 bits ADC value
  } else {
    adc_value = 0;
  }
  return adc_value >> 2;
}

void sample_dual_usf(unsigned int r) {  // dual channel
  R_ADC0->ADANSA_b[0].ANSA0 = 1;        // A1 port target
  R_ADC0->ADANSA_b[0].ANSA1 = 1;        // A2 port target
  unsigned long st = micros();
  for (int i = 0; i < SAMPLES; i++) {
    while (micros() - st < r) ;
    R_ADC0->ADCSR_b.ADST = 1;  // start conversion
    while (R_ADC0->ADCSR_b.ADST);   // wait complete flag
    cap_buf[i] = R_ADC0->ADDR[0] >> 2;
    cap_buf1[i] = R_ADC0->ADDR[1] >> 2;
    st += r;
  }
  scaleDataArray(ad_ch0, 0);
  scaleDataArray(ad_ch1, 0);
}

void sample_usf(void) {  // single channel full speed
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
  for (int i = 0; i < NSAMP / 2; i++) {
    R_ADC0->ADCSR_b.ADST = 1;  // start conversion
    while (R_ADC0->ADCSR_b.ADST); // wait complete flag
    p[i] = R_ADC0->ADDR[ad];  // get the 14 bits ADC value
  }
  for (int i = 0; i < NSAMP / 2; i++) {
    p[i] = p[i] >> 2;
  }
  int t = trigger_point();
  scaleDataArray(ch, t);
}

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

void adc_setup_for_dma(byte ad) {
  adc_extended_cfg_t adc_ext{};
  adc_ext.enable_adbuf = 1;
  adc_cfg_t adc_cfg{};
  adc_cfg.unit = 0;
  adc_cfg.mode = adc_timerdma ? ADC_MODE_SINGLE_SCAN : ADC_MODE_CONTINUOUS_SCAN;
  adc_cfg.resolution = ADC_RESOLUTION_12_BIT;
  adc_cfg.alignment = ADC_ALIGNMENT_RIGHT;
  adc_cfg.trigger = adc_timerdma ? ADC_TRIGGER_SYNC_ELC : ADC_TRIGGER_SOFTWARE;
  adc_cfg.p_extend = &adc_ext;
  R_ADC_Open(&adc_ctrl, &adc_cfg);
  adc_channel_cfg_t ch_cfg{};
  if (rate < RATE_DUAL)
    ch_cfg.scan_mask = (1UL << ad);
  else
    ch_cfg.scan_mask = (1UL << ADC_CHANNEL_0) | (1UL << ADC_CHANNEL_1);
  ch_cfg.sample_hold_mask = 1;
  ch_cfg.sample_hold_states = 24;
  R_ADC_ScanCfg(&adc_ctrl, &ch_cfg);
  adc_ctrl.p_reg->ADCSR_b.TRGE = adc_timerdma ? 1 : 0;
}

void dma_setup(byte ad, uint16_t *buffer) {
  memset(&dmacInfo, 0, sizeof(dmacInfo));
  dmacInfo.p_src = (void *)&adc_ctrl.p_reg->ADDR[ad];
  dmacInfo.p_dest = (void *)buffer;
  dmacInfo.length = NSAMP / 2;
  dmacInfo.transfer_settings_word_b.mode = TRANSFER_MODE_NORMAL;
  dmacInfo.transfer_settings_word_b.size = TRANSFER_SIZE_2_BYTE;
  dmacInfo.transfer_settings_word_b.irq = TRANSFER_IRQ_END;
  dmacInfo.transfer_settings_word_b.src_addr_mode = TRANSFER_ADDR_MODE_FIXED;
  dmacInfo.transfer_settings_word_b.dest_addr_mode = TRANSFER_ADDR_MODE_INCREMENTED;
  dmacExt.channel = 0;
  dmacExt.activation_source = ELC_EVENT_ADC0_SCAN_END;
  dmacCfg.p_info = &dmacInfo;
  dmacCfg.p_extend = &dmacExt;
  R_DMAC_Open((transfer_ctrl_t *)&dmacCtrl, &dmacCfg);
  R_DMAC_Reconfigure((transfer_ctrl_t *)&dmacCtrl, &dmacInfo);
  // R_DMAC_Reset((transfer_ctrl_t *)&dmacCtrl, (void *)&adc_ctrl.p_reg->ADDR[ad],
  //              (void *)cap_buf, NSAMP / 2);
  // R_DMAC_Enable((transfer_ctrl_t *)&dmacCtrl);
  if (rate < RATE_DUAL) {
    R_DMAC_Disable((transfer_ctrl_t *)&dmacCtrl1);
    return;
  }
  memset(&dmacInfo1, 0, sizeof(dmacInfo1));
  dmacInfo1.p_src = (void *)&adc_ctrl.p_reg->ADDR[ADC_CHANNEL_1];
  dmacInfo1.p_dest = (void *)cap_buf1;
  dmacInfo1.length = NSAMP / 2;
  dmacInfo1.transfer_settings_word_b.mode = TRANSFER_MODE_NORMAL;
  dmacInfo1.transfer_settings_word_b.size = TRANSFER_SIZE_2_BYTE;
  dmacInfo1.transfer_settings_word_b.irq = TRANSFER_IRQ_END;
  dmacInfo1.transfer_settings_word_b.src_addr_mode = TRANSFER_ADDR_MODE_FIXED;
  dmacInfo1.transfer_settings_word_b.dest_addr_mode = TRANSFER_ADDR_MODE_INCREMENTED;
  dmacExt1.channel = 1;
  dmacExt1.activation_source = ELC_EVENT_ADC0_SCAN_END;
  dmacCfg1.p_info = &dmacInfo1;
  dmacCfg1.p_extend = &dmacExt1;
  R_DMAC_Open((transfer_ctrl_t *)&dmacCtrl1, &dmacCfg1);
  R_DMAC_Reconfigure((transfer_ctrl_t *)&dmacCtrl1, &dmacInfo1);
}

void elc_setup() {
  static const elc_cfg_t elc_cfg = { { ELC_EVENT_NONE } };

  R_ELC_Open(&elc_ctrl, &elc_cfg);
  R_ELC_LinkSet(&elc_ctrl, ELC_PERIPHERAL_ADC0, ELC_EVENT_GPT5_COUNTER_OVERFLOW);
  R_ELC_Enable(&elc_ctrl);
}

void adctimer_setup() {
  float freq = 200000.0f;           // default 200kHz
  int8_t ch = 5;                    // GPT5
  if (rate == 1) freq = 500000.0f;  // 500kHz
  adcTimer.begin(TIMER_MODE_PERIODIC, GPT_TIMER, ch, freq, 50.0f, NULL);
  adcTimer.open();
}

void sample_dma(void) {  // single channel full speed
  byte ch, ad;
  uint16_t *p;
  if (ch0_mode == MODE_OFF && ch1_mode != MODE_OFF && rate < RATE_DUAL) {
    ch = ad_ch1;
    p = cap_buf1;
    ad = 1;
  } else {
    ch = ad_ch0;
    p = cap_buf;
    ad = 0;
  }
  p[NSAMP / 2 - 1] = 0xAAAA;
  R_DMAC_Reset((transfer_ctrl_t *)&dmacCtrl, (void *)&adc_ctrl.p_reg->ADDR[ad],
               (void *)p, NSAMP / 2);
  R_DMAC_Enable((transfer_ctrl_t *)&dmacCtrl);
  if (rate >= RATE_DUAL) {
    R_DMAC_Reset((transfer_ctrl_t *)&dmacCtrl1, (void *)&adc_ctrl.p_reg->ADDR[1],
                (void *)cap_buf1, NSAMP / 2);
    R_DMAC_Enable((transfer_ctrl_t *)&dmacCtrl1);
  }
  if (adc_timerdma) adcTimer.start();
  volatile uint16_t *end = &p[NSAMP / 2 - 1];
  while (*end == 0xAAAA)
    ;
  if (adc_timerdma) adcTimer.stop();
  int t = trigger_point();
  scaleDataArray(ch, t);
  if (rate >= RATE_DUAL)
    scaleDataArray(ad_ch1, t);
}

void adc_dmamode() {
  byte ad;
  uint16_t *p;
  if (ch0_mode == MODE_OFF && ch1_mode != MODE_OFF && rate < RATE_DUAL) {
    p = cap_buf1;
    ad = ADC_CHANNEL_1;
  } else {
    p = cap_buf;
    ad = ADC_CHANNEL_0;
  }
  if (rate > 0) {
    adc_timerdma = true;
    adc_setup_for_dma(ad);
    dma_setup(ad, p);
    elc_setup();
    adctimer_setup();
  } else {
    adc_timerdma = false;
    adc_setup_for_dma(ad);
    R_ADC_ScanStart(&adc_ctrl);
    dma_setup(ad, p);
  }
}

void adc_set_speed() {
  analogReadResolution(12);
  analogRead(A2);  // dummy read for setup
  analogRead(A1);  // dummy read for setup
  if (rate <= RATE_DMA)
    adc_dmamode();
}

void adc_arduino() {
  adc_extended_cfg_t adc_ext{};
  adc_ext.add_average_count = ADC_ADD_OFF;
  adc_ext.clearing = ADC_CLEAR_AFTER_READ_ON;
  adc_ext.enable_adbuf = 0;

  adc_cfg_t adc_cfg{};
  adc_cfg.unit = 0;
  adc_cfg.mode = ADC_MODE_SINGLE_SCAN;
  adc_cfg.resolution = ADC_RESOLUTION_14_BIT;
  adc_cfg.alignment = ADC_ALIGNMENT_RIGHT;
  adc_cfg.trigger = ADC_TRIGGER_SOFTWARE;
  adc_cfg.p_extend = &adc_ext;
  R_ADC_Open(&adc_ctrl, &adc_cfg);
  adc_channel_cfg_t ch_cfg{};
  ch_cfg.scan_mask = (1UL << 0);
  ch_cfg.sample_hold_mask = 1;
  ch_cfg.sample_hold_states = 24;
  R_ADC_ScanCfg(&adc_ctrl, &ch_cfg);
  adc_ctrl.p_reg->ADCSR_b.TRGE = 0;
}
