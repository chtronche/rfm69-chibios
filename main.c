/*
    ChibiOS - Copyright (C) 2006..2016 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

/*
 * Maximum speed SPI configuration (21MHz, CPHA=0, CPOL=0, MSb first).
 */
/* static const SPIConfig hs_spicfg = { */
/*   NULL, */
/*   GPIOB, */
/*   12, */
/*   0, */
/*   0 */
/* }; */

/*
 * Low speed SPI configuration (328.125kHz, CPHA=0, CPOL=0, MSb first).
 */
static const SPIConfig ls_spicfg = {
  NULL,
  GPIOA,
  4,
  SPI_CR1_BR_2 | SPI_CR1_BR_1,
  0
};

/*
 * SPI TX and RX buffers.
 */
static uint8_t txbuf[512];
static uint8_t rxbuf[512];

/*
 * SPI bus contender 1.
 */
/* static THD_WORKING_AREA(spi_thread_1_wa, 256); */
/* static THD_FUNCTION(spi_thread_1, p) { */

/*   (void)p; */
/*   chRegSetThreadName("SPI thread 1"); */
/*   while (true) { */
/*     spiAcquireBus(&SPID2);              /\* Acquire ownership of the bus.    *\/ */
/*     palSetPad(GPIOD, GPIOD_LED5);       /\* LED ON.                          *\/ */
/*     spiStart(&SPID2, &hs_spicfg);       /\* Setup transfer parameters.       *\/ */
/*     spiSelect(&SPID2);                  /\* Slave Select assertion.          *\/ */
/*     spiExchange(&SPID2, 512, */
/*                 txbuf, rxbuf);          /\* Atomic transfer operations.      *\/ */
/*     spiUnselect(&SPID2);                /\* Slave Select de-assertion.       *\/ */
/*     spiReleaseBus(&SPID2);              /\* Ownership release.               *\/ */
/*   } */
/* } */

/*
 * SPI bus contender 2.
 */
/* static THD_WORKING_AREA(spi_thread_2_wa, 256); */
/* static THD_FUNCTION(spi_thread_2, p) { */

/*   (void)p; */
/*   chRegSetThreadName("SPI thread 2"); */
/*   while (true) { */
/*     spiAcquireBus(&SPID2);              /\* Acquire ownership of the bus.    *\/ */
/*     palClearPad(GPIOD, GPIOD_LED5);     /\* LED OFF.                         *\/ */
/*     spiStart(&SPID2, &ls_spicfg);       /\* Setup transfer parameters.       *\/ */
/*     spiSelect(&SPID2);                  /\* Slave Select assertion.          *\/ */
/*     spiExchange(&SPID2, 512, */
/*                 txbuf, rxbuf);          /\* Atomic transfer operations.      *\/ */
/*     spiUnselect(&SPID2);                /\* Slave Select de-assertion.       *\/ */
/*     spiReleaseBus(&SPID2);              /\* Ownership release.               *\/ */
/*   } */
/* } */

static void print(const char *s) {
  sdWrite(&SD2, (const uint8_t *)s, strlen(s));
}

static volatile uint8_t _reg;
static volatile uint8_t v;

static uint8_t readReg(uint8_t reg) {
  spiAcquireBus(&SPID1);
  spiStart(&SPID1, &ls_spicfg);       /* Setup transfer parameters.       */
  spiSelect(&SPID1);
  _reg = reg & 0x7f;
  spiSend(&SPID1, 1, (void *)&_reg);
  spiReceive(&SPID1, 1, (void *)&v);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  return v;
}

int main(void) {
  halInit();
  chSysInit();

  sdStart(&SD2, NULL);

  /*
   * SPI1 I/O pins setup.
   */
  palSetPadMode(GPIOA, 5, PAL_MODE_ALTERNATE(5) |
                           PAL_STM32_OSPEED_HIGHEST);       /* New SCK.     */
  palSetPadMode(GPIOA, 6, PAL_MODE_ALTERNATE(5) |
                           PAL_STM32_OSPEED_HIGHEST);       /* New MISO.    */
  palSetPadMode(GPIOA, 7, PAL_MODE_ALTERNATE(5) |
                           PAL_STM32_OSPEED_HIGHEST);       /* New MOSI.    */
  palSetPadMode(GPIOA, 4, PAL_MODE_OUTPUT_PUSHPULL |
                           PAL_STM32_OSPEED_HIGHEST);       /* New CS.      */
  palSetPad(GPIOA, 4);

  chThdSleepMilliseconds(4000);
  print("Starting...\n");

  char buffer[64];

  for(;;) {
    for(uint8_t reg = 0x1; reg <= 0x4f; reg++) {
      uint8_t v = readReg(reg);
      sprintf(buffer, "%02x %02x  ", reg, v);
      print(buffer);
      if (reg % 10 == 0) print("\n");
    }
    print("\n\n");
    chThdSleepMilliseconds(3 * 1000);
  }
  return 0;
}
