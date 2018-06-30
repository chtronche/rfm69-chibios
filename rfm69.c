#include <stdio.h>
#include <string.h>

#include "rfm69.h"
#include "RFM69registers.h"

#include <ch.h>

#define RFM69_MAX_DRIVER_NUM (1)

RFM69Driver RFM69D1;

void rfm69ObjectInit(RFM69Driver *devp) {
  devp->config = NULL;
  devp->waitingThread = NULL;
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

static void rfm69SpiSend(RFM69Driver *devp, uint16_t len, const uint8_t *array) { /* No check */
  SPIDriver *spi = devp->config->spip;
  spiAcquireBus(spi);
  spiSelect(spi);
  spiSend(spi, len, array);
  spiUnselect(spi);
  spiReleaseBus(spi);
}

void rfm69WriteReg(RFM69Driver *devp, uint8_t addr, uint8_t v) {
  uint8_t array[2] = { addr | 0x80, v };
  rfm69SpiSend(devp, 2, array);
}

const rfm69_frequency_t rfm69_315MHz = { 0x4E, 0xC0, 0x00 };
const rfm69_frequency_t rfm69_433MHz = { 0x6C, 0x40, 0x00 };
const rfm69_frequency_t rfm69_868MHz = { 0xD9, 0x00, 0x00 };
const rfm69_frequency_t rfm69_915MHz = { 0xE4, 0xC0, 0x00 };

void print(const char *s);

static inline void setMode(RFM69Driver *devp, uint8_t mode) {
  rfm69WriteReg(devp, RFM69_REG_OPMODE, mode);
}

void rfm69SetFrequency(RFM69Driver *devp, const rfm69_frequency_t *freq) {
  setMode(devp, RFM69_RF_OPMODE_STANDBY);
  rfm69WriteReg(devp, RFM69_REG_FRFMSB, freq->msb);
  rfm69WriteReg(devp, RFM69_REG_FRFMID, freq->mid);
  rfm69WriteReg(devp, RFM69_REG_FRFLSB, freq->lsb);
}

const rfm69_bitrate_t rfm69_4800bps = { 0x02, 0x40 };
const rfm69_bitrate_t rfm69_9600bps = { 0x0d, 0x05 };

void rfm69SetBitrate(RFM69Driver *devp, const rfm69_bitrate_t *br) {
  setMode(devp, RFM69_RF_OPMODE_STANDBY);
  rfm69WriteReg(devp, RFM69_REG_BITRATEMSB, br->msb);
  rfm69WriteReg(devp, RFM69_REG_BITRATELSB, br->lsb);
}

static void rfm69SetHighPowerRegs(RFM69Driver *devp, bool _set) {
  (void)devp;
  (void) _set;
  /* What is powerful for now is this function :-) */
}

static void rfm69SetMode(RFM69Driver *devp, uint8_t newMode) {
  if (devp->mode == newMode) return;
  rfm69WriteReg(devp, RFM69_REG_OPMODE, (rfm69ReadReg(devp, RFM69_REG_OPMODE) & 0xE3) | newMode);
  devp->mode = newMode;

  if (devp->config->isRFM69HW &&
      (newMode == RFM69_RF_OPMODE_TRANSMITTER || newMode == RFM69_RF_OPMODE_RECEIVER))
    rfm69SetHighPowerRegs(devp, newMode == RFM69_RF_OPMODE_TRANSMITTER);

  // we are using packet mode, so this check is not really needed
  // but waiting for mode ready is necessary when going from sleep because the FIFO may not be immediately available from previous mode
  if (devp->mode == RFM69_RF_OPMODE_SLEEP) 
    while (!(rfm69ReadReg(devp, RFM69_REG_IRQFLAGS1) & RFM69_RF_IRQFLAGS1_MODEREADY)); // wait for ModeReady
}

/* Same default settings as in the LowPowerLab library, to get easier interoperability */

static const uint8_t _default[][2] = {
  /* 0x05 */ { RFM69_REG_FDEVMSB, RFM69_RF_FDEVMSB_50000 } , // default: 5KHz, (FDEV + BitRate / 2 <= 500KHz)
  /* 0x06 */ { RFM69_REG_FDEVLSB, RFM69_RF_FDEVLSB_50000 },
  /* 0x19 */ { RFM69_REG_RXBW, RFM69_RF_RXBW_DCCFREQ_010 | RFM69_RF_RXBW_MANT_16 | RFM69_RF_RXBW_EXP_2 }, // (BitRate < 2 * RxBw)
  /* 0x29 */ { RFM69_REG_RSSITHRESH, 220 }, // must be set to dBm = (-Sensitivity / 2, default is 0xE4
  /* 0x2E */ { RFM69_REG_SYNCCONFIG, RFM69_RF_SYNC_ON | RFM69_RF_SYNC_FIFOFILL_AUTO | RFM69_RF_SYNC_SIZE_2 | RFM69_RF_SYNC_TOL_0 },
  /* 0x2F */ { RFM69_REG_SYNCVALUE1, 0x2D },      // attempt to make this compatible with sync1 byte of  RFM12B lib
  /* 0x30 */ { RFM69_REG_SYNCVALUE2, 21 }, // NETWORK ID
  /* 0x37 */ { RFM69_REG_PACKETCONFIG1, RFM69_RF_PACKET1_FORMAT_VARIABLE | RFM69_RF_PACKET1_DCFREE_OFF | RFM69_RF_PACKET1_CRC_ON | RFM69_RF_PACKET1_CRCAUTOCLEAR_ON | RFM69_RF_PACKET1_ADRSFILTERING_OFF },
  /* 0x38 */ { RFM69_REG_PAYLOADLENGTH, 66 }, // in variable length mode: the max frame size, not used     
  /* 0x3C */ { RFM69_REG_FIFOTHRESH, RFM69_RF_FIFOTHRESH_TXSTART_FIFONOTEMPTY | RFM69_RF_FIFOTHRESH_VALUE }, // TX on FIFO not empty
  /* 0x3D */ { RFM69_REG_PACKETCONFIG2, RFM69_RF_PACKET2_RXRESTARTDELAY_2BITS | RFM69_RF_PACKET2_AUTORXRESTART_ON | RFM69_RF_PACKET2_AES_OFF }, // RXRESTARTDELAY must match transmitter PA ramp-down time (bitrate dependent)
  /* 0x6F */ { RFM69_REG_TESTDAGC, RFM69_RF_DAGC_IMPROVED_LOWBETA0 }, // run DAGC continuously in RX mode for Fading Margin Improvement, recommended default for AfcLowBetaOn=0
  { 0xff, 0 }
};

void rfm69Start(RFM69Driver *devp, RFM69Config *config) {
  if (devp->config) return; /* Started already */
  devp->config = config;
  devp->waitingThread = NULL;
  devp->status = rfm69_status_ok; /* We'll change that if it's wrong */
  devp->mode = -1; /* Aka unitialized */
  devp->rxEmpty = true;
  devp->rxAvailable = 0;

  SPIDriver *spi = config->spip;
  spiAcquireBus(spi);
  spiStart(spi, config->spiConfig);
  spiReleaseBus(spi);

  rfm69WriteReg(devp, RFM69_REG_DIOMAPPING1, RFM69_RF_DIOMAPPING1_DIO0_01);  /* Spend most of the time receiving, so listen to an interrupt "packet received" */
  rfm69WriteReg(devp, RFM69_REG_DIOMAPPING2, RFM69_RF_DIOMAPPING2_CLKOUT_OFF);  /* DIO0 is the only IRQ we're using */

  rfm69SetFrequency(devp, config->frequency);
  if (!config->bitrate) config->bitrate = &rfm69_4800bps;
  rfm69SetBitrate(devp, config->bitrate);
  /*rfm69SetEncryption(NULL);
   */

  for(const uint8_t (*p)[2] = _default; (*p)[0] != 0xff; p++) {
    rfm69WriteReg(devp, (*p)[0], (*p)[1]);
  }

  rfm69SetMode(devp, RFM69_RF_OPMODE_RECEIVER);
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

void rfm69Reset(ioportid_t ioport, uint16_t pad) {
  palSetPadMode(ioport, pad, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPad(ioport, pad);
  chThdSleepMicroseconds(100);
  palClearPad(ioport, pad);
  chThdSleepMilliseconds(5);
}

void extCallback(RFM69Driver *devp) {
  led(3);
  chSysLockFromISR();
  if (devp->mode == RFM69_RF_OPMODE_RECEIVER) devp->rxEmpty = false;
  if (devp->waitingThread) {
    // tp->p_u.rdymsg = (msg_t)55;     /* Sending the message, optional.*/
    chSchReadyI(devp->waitingThread);
    devp->waitingThread = NULL;
  }
  chSysUnlockFromISR();
  led(4);
}

void rfm69D1ExtCallback(EXTDriver *extp, expchannel_t channel) {
  (void)extp;
  (void)channel;

  extCallback(&RFM69D1);
}

#define BUFFER_SIZE (64)

char buffer[BUFFER_SIZE];

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void discard(RFM69Driver *devp, unsigned int n) {
  SPIDriver *spi = devp->config->spip;
  spiAcquireBus(spi);
  spiSelect(spi);

  uint8_t readFifo = RFM69_REG_FIFO;
  if (n > devp->rxAvailable) n = devp->rxAvailable;
  while(n > 0) {
    unsigned int toRead = MIN(n, BUFFER_SIZE);
    spiExchange(spi, toRead, &readFifo, buffer);
    devp->rxAvailable -= toRead;
    n -= toRead;
  }

  if (!devp->rxAvailable) devp->rxEmpty = true;
  spiUnselect(spi);
  spiReleaseBus(spi);
}

void clearFIFO(RFM69Driver *devp) {
}

static void _rfm69ReadAvailable(RFM69Driver *devp) {
  if (devp->rxEmpty) return;
  if (devp->rxAvailable) return;
  devp->rxAvailable = rfm69ReadReg(devp, RFM69_REG_FIFO) + 1;  /* +1 because of length ? */
  /* sprintf(buffer, "len = %d\n", devp->rxAvailable); */
  /* print(buffer); */
  if (devp->rxAvailable < 2) {
    /* print("Bad packet length ???\n"); */
    discard(devp,devp->rxAvailable);
    return;
  }
  if (devp->config->lowPowerLabCompatibility) {
    if (devp->rxAvailable < 5) {
      /* print("LPL Bad packet length ???\n"); */
      discard(devp,devp->rxAvailable);
      return;
    }
    devp->lpl_targetId = rfm69ReadReg(devp, RFM69_REG_FIFO);
    devp->senderId = rfm69ReadReg(devp, RFM69_REG_FIFO);
    devp->lpl_ctl = rfm69ReadReg(devp, RFM69_REG_FIFO);
    devp->rxAvailable -= 3;
  }
}

unsigned int rfm69ReadAvailable(RFM69Driver *devp) {
  _rfm69ReadAvailable(devp);
  return devp->rxAvailable;
}

static void waitForCompletion(RFM69Driver *devp) {
  palSetPad(GPIOC, 0);
    chSysLock();
    devp->waitingThread = chThdGetSelfX();
    led(5);
    chSchGoSleepS(CH_STATE_SUSPENDED);
    led(6);
    /* msg = chThdSelf()->p_u.rdymsg; */
    chSysUnlock();
  palClearPad(GPIOC, 0);
}

void rfm69Read(RFM69Driver *devp, void *buf, uint8_t bufferSize) {
  (void)devp;
  SPIDriver *spi = devp->config->spip;

  unsigned int available = rfm69ReadAvailable(devp);

  if (!available) {
    waitForCompletion(devp);
    available = rfm69ReadAvailable(devp);
  }

  uint8_t readFifo = RFM69_REG_FIFO;
  int toRead = MIN(bufferSize, available);
  sprintf(buffer, "\n%d bytes: ", toRead);
  print(buffer);

  spiAcquireBus(spi);
  spiSelect(spi);

  /*
  spiExchange(spi, 4, &readFifo, buf);
  */
  spiExchange(spi, toRead, &readFifo, buf);

  spiUnselect(spi);
  spiReleaseBus(spi);

  if (toRead > 32) {
    const char *p = buf;
    for(int n = toRead; n; --n) {
      sprintf(buffer, "%02x ", *p++);
      print(buffer);
    }
  }

  if (toRead < bufferSize) ((char *)buf)[toRead] = '\0';
  devp->rxAvailable -= toRead;
  if (!devp->rxAvailable) {
    devp->rxEmpty = true;
    rfm69SetMode(devp, RFM69_RF_OPMODE_RECEIVER);
  }

  sprintf(buffer, "read %d chars, %d left, empty=%d: \"%s\"\n",
  	  toRead, devp->rxAvailable, devp->rxEmpty, (char *)(buf + 4));
  print(buffer);
}

uint8_t rfm69ReadRSSI(RFM69Driver *devp) {
  return rfm69ReadReg(devp, RFM69_REG_RSSIVALUE);
}

static void dumpState(RFM69Driver *devp, const char *pos) {
  uint8_t mode = rfm69ReadReg(devp, 0x01);
  uint8_t data = rfm69ReadReg(devp, 0x02);
  uint8_t irq1 = rfm69ReadReg(devp, 0x27);
  uint8_t irq2 = rfm69ReadReg(devp, 0x28);
  sprintf(buffer, "%s\tmode=%x\tdata=%x\tirq1=%x\tirq2=%x\n", pos, mode, data, irq1, irq2);
  print(buffer);
}

void rfm69Send(RFM69Driver *devp, uint8_t bufferSize, const void *buf) {
  sprintf(buffer, "Trying to send %d/%s...", bufferSize, (const char *)buf + 4);
  print(buffer);
  led(7);
  rfm69SetMode(devp, RFM69_RF_OPMODE_STANDBY); /* So as not to be disturbed by arriving packet */
  /* avoid RX deadlocks */
  rfm69WriteReg(devp, RFM69_REG_PACKETCONFIG2,
		(rfm69ReadReg(devp, RFM69_REG_PACKETCONFIG2) & 0xFB) | RFM69_RF_PACKET2_RXRESTART);

  dumpState(devp, "before wr fifo");
  rfm69WriteReg(devp, RFM69_REG_FIFO, bufferSize);
  rfm69SpiSend(devp, bufferSize, buf);
  dumpState(devp, "fifo written");
  //dumpState(devp, "Transmit on");

  /*
  print("Buffer read back [");
  for(int n = 63; n; --n) {
    uint8_t status = rfm69ReadReg(devp, 0x28);
    sprintf(buffer, "-%02x ", status);
    print(buffer);
    
    if (!(status & 0x40)) break;
    uint8_t v = rfm69ReadReg(devp, 0);
    sprintf(buffer, "%02x ", v);
    print(buffer);
  }
  print("]\n");
  rfm69WriteReg(devp, RFM69_REG_FIFO, bufferSize);
  rfm69SpiSend(devp, bufferSize, buf);
  */

  rfm69WriteReg(devp, RFM69_REG_DIOMAPPING1, RFM69_RF_DIOMAPPING1_DIO0_00); /* Interrupt when packet xmitted */
  rfm69SetMode(devp, RFM69_RF_OPMODE_TRANSMITTER);

  waitForCompletion(devp);
  dumpState(devp, "transmitted");

  rfm69WriteReg(devp, RFM69_REG_DIOMAPPING1, RFM69_RF_DIOMAPPING1_DIO0_01); /* Interrupt when packet received */
  rfm69SetMode(devp, RFM69_RF_OPMODE_RECEIVER);
  dumpState(devp, "Listening...");

  print("done\n");
}

struct driverLine {
  int mask;
  stm32_gpio_t *port;
  int bit;
} _driver[4] = {
  { 0x1, GPIOA, 4 },
  { 0x2, GPIOB, 0 },
  { 0x3, GPIOC, 1 },
  { 0x4, GPIOC, 0 },
  { 0, 0, 0 }
};


void led(int n) {
  if (n & 0x1) palSetPad(GPIOA, 4); else palClearPad(GPIOA, 4);
  if (n & 0x2) palSetPad(GPIOB, 0); else palClearPad(GPIOB, 0);
  if (n & 0x4) palSetPad(GPIOC, 1); else palClearPad(GPIOC, 1);
  if (n & 0x8) palSetPad(GPIOC, 0); else palClearPad(GPIOC, 0);
}
