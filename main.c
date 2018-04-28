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

static const SPIConfig ls_spicfg = {
  NULL,
  GPIOB,
  6,
  SPI_CR1_BR_2, /* Should be about 1.2 MHz (fPCLK / 32) on STM32F4. Doc says RFM69 supports 10 MHz SPI communication, but people on internet say they had problem at 1 MHz. */
  0
};

RFM69Config _RFM69Config = {
  &SPID1, &ls_spicfg,
  GPIOB, 6, /* slave select */
  &rfm69_433MHz,
  &rfm69_4800bps,
  false, /* isRFM69HW */
  false, /* lowPowerLabCompatibility */
};

static const EXTConfig extcfg = {
  {
    {EXT_CH_MODE_DISABLED, NULL}, /* 0 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 1 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 2 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 3 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 4 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 5 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 6 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 7 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 8 */
    {EXT_CH_MODE_RISING_EDGE | EXT_MODE_GPIOA | EXT_CH_MODE_AUTOSTART, rfm69_1ExtCallback }, /* 9 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 10 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 11 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 12 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 13 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 14 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 15 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 16 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 17 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 18 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 19 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 20 */
    {EXT_CH_MODE_DISABLED, NULL}, /* 21 */
    {EXT_CH_MODE_DISABLED, NULL}  /* 22 */
  }
};

int main(void) {
  halInit();
  chSysInit();

  sdStart(&SD2, NULL);

  rfm69ObjectInit(&RFM69_1);

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
  extStart(&EXTD1, &extcfg); /* Don't start before rfm69Start ! Or enable channel only after */
  rfm69Start(&RFM69_1, &_RFM69Config);

  char buffer[64];

  for(uint8_t reg = 0x1; reg <= 0x4f; reg++) {
    uint8_t v = rfm69ReadReg(&RFM69_1, reg);
    sprintf(buffer, "%02x %02x  ", reg, v);
    print(buffer);
    if (reg % 10 == 0) print("\n");
  }
  print("\n");

  for(;;) {
    print(".");
    /* uint8_t mode = rfm69ReadReg(&RFM69_1, 0x01); */
    /* uint8_t flags1 = rfm69ReadReg(&RFM69_1, 0x27); */
    /* uint8_t flags2 = rfm69ReadReg(&RFM69_1, 0x28); */
    /* sprintf(buffer, "[%x %x %x]", mode, flags1, flags2); */
    /* print(buffer); */
    if (rfm69ReadAvailable(&RFM69_1)) {
      rfm69Read(&RFM69_1, buffer, 64);
      print(buffer + 4);
      sprintf(buffer, "rssi = %d\n", rfm69ReadRSSI(&RFM69_1));
    }
    chThdSleepMilliseconds(500);
  }
  return 0;
}
