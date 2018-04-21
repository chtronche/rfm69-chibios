#include <stdio.h>
#include <string.h>

#include "rfm69.h"
#include "RFM69registers.h"

#include <ch.h>

void rfm69ObjectInit(RFM69Driver *devp) {
  devp->config = NULL;
  devp->status = rfm69_status_stop;
}

uint8_t rfm69ReadReg(RFM69Driver *devp, uint8_t addr) {
  SPIDriver *spi = devp->config->spip;
  spiAcquireBus(spi);
  spiSelect(spi);
  spiSend(spi, 1, (void *)&addr);
  uint8_t v;
  spiReceive(spi, 1, (void *)&v);
  spiUnselect(spi);
  spiReleaseBus(spi);
  return v;
}

void rfm69WriteReg(RFM69Driver *devp, uint8_t addr, uint8_t v) {
  SPIDriver *spi = devp->config->spip;
  spiAcquireBus(spi);
  spiSelect(spi);
 uint8_t array[2] = { addr | 0x80, v };
  spiSend(spi, 2, array);
  spiUnselect(spi);
  spiReleaseBus(spi);
}

static uint8_t freq_315MHz[] = { 0x4E, 0xC0, 0x00 };
static uint8_t freq_433MHz[] = { 0x6C, 0x40, 0x00 };
static uint8_t freq_868MHz[] = { 0xD9, 0x00, 0x00 };
static uint8_t freq_915MHz[] = { 0xE4, 0xC0, 0x00 };

static void print(const char *s) {
  sdWrite(&SD2, (const uint8_t *)s, strlen(s));
}

static inline void setMode(RFM69Driver *devp, uint8_t mode) {
  rfm69WriteReg(devp, RFM69_REG_OPMODE, mode);
}

void rfm69SetFrequency(RFM69Driver *devp, rfm69_frequency_t freq) {
  setMode(devp, RFM69_RF_OPMODE_STANDBY);
  static uint8_t *p;
  switch(freq) {
  case RFM69_315MHZ: p = freq_315MHz; break;
  case RFM69_433MHZ: p = freq_433MHz; break;
  case RFM69_868MHZ: p = freq_868MHz; break;
  case RFM69_915MHZ: p = freq_915MHz; break;
  default:
    devp->status = rfm69_status_bad_frequency;
    return;
  }
  
  print("ok\n");
  rfm69WriteReg(devp, RFM69_REG_FRFMSB, p[0]);
  rfm69WriteReg(devp, RFM69_REG_FRFMID, p[1]);
  rfm69WriteReg(devp, RFM69_REG_FRFLSB, p[2]);
}

void rfm69Start(RFM69Driver *devp, RFM69Config *config) {
  if (devp->config) return; /* Started already */
  devp->config = config;
  SPIDriver *spi = config->spip;
  spiAcquireBus(spi);
  spiStart(spi, config->spiConfig);
  spiReleaseBus(spi);
  rfm69SetFrequency(devp, config->frequency);
  /*rfm69SetEncryption(NULL);
   */
  rfm69SetMode(RFM69_RF_OPMODE_STANDBY);
}

void rfm69Stop(RFM69Driver *devp) {
  if (!devp->config) return;
  SPIDriver *spi = devp->config->spip;
  spiAcquireBus(spi);
  spiStop(spi);
  spiReleaseBus(spi);
  devp->config = NULL;
  devp->status = rfm69_status_stop;
}

void rfm69Read(RFM69Driver *devp, void *buffer, uint8_t bufferSize) {
  
}

void rfm69Reset(ioportid_t ioport, uint16_t pad) {
  palSetPadMode(ioport, pad, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPad(ioport, pad);
  chThdSleepMicroseconds(100);
  palClearPad(ioport, pad);
  chThdSleepMilliseconds(5);
}
