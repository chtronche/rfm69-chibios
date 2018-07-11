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
  { GPIOA, 4 }, // 0x1 red
  { GPIOB, 0 }, // 0x2 yellow
  { GPIOC, 1 }, // 0x3 blue
  { GPIOC, 0 }, // 0x4 white
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
