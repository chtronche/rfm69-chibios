#include <stdio.h>
#include <string.h>

#include "debug.h"

#include "hal.h"

void print(const char *s) {
  //  chMtxLock(&printMutex);
  sdWrite(&SD2, (const uint8_t *)s, strlen(s));
  //  chMtxUnlock(&printMutex);
}

/********************************************************************************/

struct driverLine {
  stm32_gpio_t *port;
  int bit;
} _driver[] = {
  { GPIOA, 11 }, // 0x1 red
  { GPIOA, 12 }, // 0x2 yellow
  { GPIOC, 5 }, // 0x3 blue
  { GPIOC, 6 }, // 0x4 white
  { GPIOC, 8 }, // 0x4 white
  { 0, 0 }
};

void led(int n, unsigned mask) {
  unsigned bit = 1;
  for(struct driverLine *p = _driver; p->port; p++) {
    if (mask & bit) {
      if (n & bit) palSetPad(p->port, p->bit); else palClearPad(p->port, p->bit);
    }
    bit = bit << 1;
  }
}

void init_led() {
  for(struct driverLine *p = _driver; p->port; p++) {
    palSetPadMode(p->port, p->bit, PAL_MODE_OUTPUT_PUSHPULL);
  }
}

void chenillard() {
  unsigned v = 1;
  for(int n = 5; n; n--) {
    led(v, 0x1f);
    v = v << 1;
    chThdSleepMilliseconds(200);
  }
  led(0, 0x1f);
}

uint8_t regs[] = { 0x1, 0x2, 0x3, 0x4, 0x25, 0x26, 0x27, 0x28, 0 };

static char buffer[64];

void dumpReg(RFM69Driver *devp) {
  for(uint8_t *p = regs; *p; p++) {
    uint8_t v = rfm69ReadReg(devp, *p);
    sprintf(buffer, "%02x=%02x ", *p, v);
    print(buffer);
  }
  print("\n");
}

static void dumpState(RFM69Driver *devp, const char *pos) {
  uint8_t mode = rfm69ReadReg(devp, 0x01);
  uint8_t data = rfm69ReadReg(devp, 0x02);
  uint8_t irq1 = rfm69ReadReg(devp, 0x27);
  uint8_t irq2 = rfm69ReadReg(devp, 0x28);
  sprintf(buffer, "%s\tmode=%x\tdata=%x\tirq1=%x\tirq2=%x\n", pos, mode, data, irq1, irq2);
  print(buffer);
}

