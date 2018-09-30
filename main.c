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
#include <stdlib.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "rfm69.h"

#include "debug.h"

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
    {EXT_CH_MODE_RISING_EDGE | EXT_MODE_GPIOA | EXT_CH_MODE_AUTOSTART, rfm69D1ExtCallback }, /* 9 */
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

/*
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {
  (void)arg;
  chRegSetThreadName("check");
  for(int n = 0;;++n) {
    palTogglePad(GPIOC, 0);
    chThdSleepSeconds(1);
  }
}
*/

static volatile int n = 0;

static uint32_t total = 0;
static uint32_t missed = 0;
static uint32_t last = 0;

static void compareReg(const char *buffer) {
  const char *p = index(buffer, '-');
  if (!p) return;
  char *end;
  unsigned reg = strtoul(p + 1, &end, 16);
  unsigned v = strtoul(end +1, NULL, 16);

  unsigned vv = rfm69ReadReg(&RFM69D1, reg);

  if (v == vv) return;

  char buffer2[32];
  sprintf(buffer2, "***<%02x|%02x>***", v, vv);
  print(buffer2);
}

char buffer3[64];

static void _dump(const char *buffer) {
  for(int line = 8; line; line--) {
    for(int col = 8; col; col--) {
      sprintf(buffer3, "%02x ", *buffer++);
      print(buffer3);
    }
    print("\n");
  }
}

static void closeString(char *buffer) {
  int max = 64;
  unsigned char *p = (unsigned char *)buffer + 4;
  while(--max && *p >= ' ') p++;
  *p = '\0';
}

int main(void) {
  halInit();
  chSysInit();

  //chMtxInit(printMutex);
  sdStart(&SD2, NULL);

  print("Starting v2\n");
  //  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  rfm69ObjectInit(&RFM69D1);

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

  palSetPadMode(GPIOA, 11, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOA, 12, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOC, 5, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOC, 6, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOC, 8, PAL_MODE_OUTPUT_PUSHPULL);

  chenillard();
  rfm69Reset(GPIOC, 7);
  led(1, 0x1f);

  extStart(&EXTD1, &extcfg); /* Don't start before rfm69Start ! Or enable channel only after */
  led(2, 0x1f);
  rfm69Start(&RFM69D1, &_RFM69Config);
  led(3, 0x1f);

  rfm69WriteReg(&RFM69D1, 0x13, 0xf);
  rfm69WriteReg(&RFM69D1, 0x11, 0x7f);

  char buffer[64];

  for(uint8_t reg = 0x1; reg <= 0x4f; reg++) {
    uint8_t v = rfm69ReadReg(&RFM69D1, reg);
    sprintf(buffer, "%02x %02x  ", reg, v);
    print(buffer);
    if (reg % 10 == 0) print("\n");
  }
  print("\n");

  for(;;) {
    led(4, 0x1f);
    rfm69Read(&RFM69D1, buffer, 64);
    led(5, 0x1f);
    total += 1;
    closeString(buffer);
    //_dump(buffer);
    print(">>>");
    print(buffer + 4);
    print("\n");
    uint32_t v = atoi(buffer + 5);
    if (v) {
      if (last && v > last) missed += (v - last - 1);
      last = v;
    }
    
    compareReg(buffer + 4);
    sprintf(buffer, "rssi = %d\tv=%ld\ttotal=%ld\tmissed=%ld\n", 
	    rfm69ReadRSSI(&RFM69D1), v, total, missed);
    print(buffer);
    //}
    chThdSleepMilliseconds(250);

    buffer[0] = '\x80';
    buffer[1] = (char)(98); /* Target id */
    buffer[2] = 'P'; /* Source id */
    buffer[3] = '\0'; /* Control byte */
    sprintf(buffer + 4, "coucou-xxxx-yyyy %d", n++);
    rfm69Send(&RFM69D1, strlen(buffer + 4) + 4, buffer);
    chThdSleepMilliseconds(250);
  }
  return 0;
}
