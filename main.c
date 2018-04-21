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

#include "rfm69.h"

static void print(const char *s) {
  sdWrite(&SD2, (const uint8_t *)s, strlen(s));
}

/* static uint8_t readReg(uint8_t reg) { */
/*   spiAcquireBus(&SPID1); */
/*   spiStart(&SPID1, &ls_spicfg);       /\* Setup transfer parameters.       *\/ */
/*   spiSelect(&SPID1); */
/*   _reg = reg & 0x7f; */
/*   spiSend(&SPID1, 1, (void *)&_reg); */
/*   spiReceive(&SPID1, 1, (void *)&v); */
/*   spiUnselect(&SPID1); */
/*   spiReleaseBus(&SPID1); */
/*   return v; */
/* } */

static const SPIConfig ls_spicfg = {
  NULL,
  GPIOB,
  6,
  SPI_CR1_BR_2, /* Should be about 1.2 MHz (fPCLK / 32) on STM32F4. Doc says RFM69 supports 10 MHz SPI communication, but people on internet say they had problem at 1 MHz. */
  0
};

RFM69Driver RFM69;

RFM69Config _RFM69Config = {
  &SPID1, &ls_spicfg,
  GPIOB, 6, /* slave select */
  RFM69_433MHZ,
};

int main(void) {
  halInit();
  chSysInit();

  sdStart(&SD2, NULL);
  rfm69ObjectInit(&RFM69);

  /*
   * SPI1 I/O pins setup.
   */
  palSetPadMode(GPIOA, 5, PAL_MODE_ALTERNATE(5) |
                           PAL_STM32_OSPEED_HIGHEST);       /* New SCK.     */
  palSetPadMode(GPIOA, 6, PAL_MODE_ALTERNATE(5) |
                           PAL_STM32_OSPEED_HIGHEST);       /* New MISO.    */
  palSetPadMode(GPIOA, 7, PAL_MODE_ALTERNATE(5) |
                           PAL_STM32_OSPEED_HIGHEST);       /* New MOSI.    */
  palSetPadMode(GPIOB, 6, PAL_MODE_OUTPUT_PUSHPULL |
                           PAL_STM32_OSPEED_HIGHEST);       /* New CS.      */
  palSetPad(GPIOB, 6);

  rfm69Reset(GPIOC, 7);

  print("Starting...\n");
  rfm69Start(&RFM69, &_RFM69Config);

  char buffer[64];

  for(;;) {
    for(uint8_t reg = 0x1; reg <= 0x4f; reg++) {
      uint8_t v = rfm69ReadReg(&RFM69, reg);
      sprintf(buffer, "%02x %02x  ", reg, v);
      print(buffer);
      if (reg % 10 == 0) print("\n");
    }
    print("\n\n");
    chThdSleepMilliseconds(3 * 1000);
  }
  return 0;
}
