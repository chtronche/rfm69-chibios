#include <stdio.h>
#include <string.h>

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
