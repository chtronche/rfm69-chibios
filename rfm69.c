#include <stdio.h>
#include <string.h>

#include "rfm69.h"
#include "RFM69registers.h"

#include <ch.h>

#define RFM69_MAX_DRIVER_NUM (1)

/*
 * TODO: Check all spiExchange / spiReceive and check if it shouldn't be the reverse
 */

RFM69Driver RFM69D1;

void rfm69ObjectInit(RFM69Driver *devp) {
  devp->config = NULL;
  devp->waitingThread = NULL;
  devp->status = rfm69_status_stop;
  devp->badPacket = 0;
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

/* When a register is written to the RFM69, it sends the old value
   back during the SPI exchange. We could use this property to return
   the register's old value, but don't do it yet */

void rfm69WriteReg(RFM69Driver *devp, uint8_t addr, uint8_t v) {
  uint8_t array[2] = { addr | 0x80, v };
  rfm69SpiSend(devp, 2, array);
}

const rfm69_frequency_t rfm69_315MHz = { 0x4E, 0xC0, 0x00 };
const rfm69_frequency_t rfm69_433MHz = { 0x6C, 0x40, 0x00 };
const rfm69_frequency_t rfm69_868MHz = { 0xD9, 0x00, 0x00 };
const rfm69_frequency_t rfm69_915MHz = { 0xE4, 0xC0, 0x00 };

char bufferd[128];

void rfm69SetFrequency(RFM69Driver *devp, const rfm69_frequency_t *freq) {
  rfm69SetMode(devp, RFM69_RF_OPMODE_STANDBY);
  rfm69WriteReg(devp, RFM69_REG_FRFMSB, freq->msb);
  rfm69WriteReg(devp, RFM69_REG_FRFMID, freq->mid);
  rfm69WriteReg(devp, RFM69_REG_FRFLSB, freq->lsb);
}

const rfm69_bitrate_t rfm69_4800bps = { 0x02, 0x40 };
const rfm69_bitrate_t rfm69_9600bps = { 0x0d, 0x05 };

void rfm69SetBitrate(RFM69Driver *devp, const rfm69_bitrate_t *br) {
  rfm69SetMode(devp, RFM69_RF_OPMODE_STANDBY);
  rfm69WriteReg(devp, RFM69_REG_BITRATEMSB, br->msb);
  rfm69WriteReg(devp, RFM69_REG_BITRATELSB, br->lsb);
}

static void rfm69SetHighPowerRegs(RFM69Driver *devp, bool _set) {
  (void)devp;
  (void) _set;
  /* What is powerful for now is this function :-) */
}

void rfm69SetMode(RFM69Driver *devp, uint8_t newMode) {
  if (devp->mode == newMode) return;

  switch (newMode) {
  case RFM69_RF_OPMODE_RECEIVER:
    /* Interrupt when packet received */
    rfm69WriteReg(devp, RFM69_REG_DIOMAPPING1, RFM69_RF_DIOMAPPING1_DIO0_01);
    break;
  case RFM69_RF_OPMODE_TRANSMITTER:
    /* Interrupt when packet xmitted */    
    rfm69WriteReg(devp, RFM69_REG_DIOMAPPING1, RFM69_RF_DIOMAPPING1_DIO0_00);
    break;
  }

  rfm69WriteReg(devp, RFM69_REG_OPMODE, (rfm69ReadReg(devp, RFM69_REG_OPMODE) & 0xE3) | newMode);
  devp->mode = newMode;

  if (devp->config->isRFM69HW &&
      (newMode == RFM69_RF_OPMODE_TRANSMITTER || newMode == RFM69_RF_OPMODE_RECEIVER))
    rfm69SetHighPowerRegs(devp, newMode == RFM69_RF_OPMODE_TRANSMITTER);

  /*
    We don't wait when going to reading mode, because when doing so,
    the next step is to wait for a packet, that we'll get only the the
    RFM69's ready. 
    We don't wait when going to writing mode, because after that,
    we'll just send a packet, and the RFM69 won't do it before being ready.  
  */
  if (newMode == RFM69_RF_OPMODE_SLEEP)
    while (!(rfm69ReadReg(devp, RFM69_REG_IRQFLAGS1) & RFM69_RF_IRQFLAGS1_MODEREADY));
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

  rfm69SetMode(&RFM69D1, RFM69_RF_OPMODE_STANDBY);
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
  led(0x10, 0x10);
  chSysLockFromISR();
  if (devp->mode == RFM69_RF_OPMODE_RECEIVER) devp->rxEmpty = false;
  if (!devp->rxEmpty) led(0x1, 0x1);
  if (devp->waitingThread) {
    // tp->p_u.rdymsg = (msg_t)55;     /* Sending the message, optional.*/
    chSchReadyI(devp->waitingThread);
    devp->waitingThread = NULL;
  }
  chSysUnlockFromISR();
  led(0, 0x10);
}

unsigned long nExt = 0;
void *lastAddr = 0;

void rfm69D1ExtCallback(EXTDriver *extp, expchannel_t channel) {
  (void)extp;
  (void)channel;

  nExt++;
  lastAddr = &RFM69D1;
  extCallback(&RFM69D1);
}

#define BUFFER_SIZE (128)

char buffer[BUFFER_SIZE];

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void _rfm69ReadAvailable(RFM69Driver *devp) {
  if (devp->rxEmpty) return;
  if (devp->rxAvailable) return;
  devp->rxAvailable = rfm69ReadReg(devp, RFM69_REG_FIFO) + 1;  /* +1 because of length ? */
  if (devp->rxAvailable < 2 || devp->rxAvailable > 64) {
    print("*** Reset queue ***\n");
    devp->badPacket++;
    rfm69ClearFIFO(devp);
    return;
  }
  if (devp->config->lowPowerLabCompatibility) {
    if (devp->rxAvailable < 5) {
      /* print("LPL Bad packet length ???\n"); */
      print("*** Reset queue/2 ***\n");
      devp->badPacket++;
      rfm69ClearFIFO(devp);
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
  led(4, 6);
  chSysLock();
  devp->waitingThread = chThdGetSelfX();
  led(5, 6);
  chSchGoSleepS(CH_STATE_SUSPENDED);
  /* msg = chThdSelf()->p_u.rdymsg; */
  chSysUnlock();
  led(0, 6);
}

void rfm69Read(RFM69Driver *devp, void *buf, uint8_t bufferSize) {
  (void)devp;
  SPIDriver *spi = devp->config->spip;

  led(6, 0x1f);
  unsigned int available = rfm69ReadAvailable(devp);
  led(7, 0x1f);

  if (!available) {
    led(8, 0x1f);
    waitForCompletion(devp);
    led(9, 0x1f);
    available = rfm69ReadAvailable(devp);
    led(10, 0x1f);
  }
  led(11, 0x1f);

  uint8_t readFifo = RFM69_REG_FIFO;
  int toRead = MIN(bufferSize, available);

  spiAcquireBus(spi);
  spiSelect(spi);

  led(12, 0x1f);
  spiExchange(spi, toRead, &readFifo, buf);
  led(13, 0x1f);

  spiUnselect(spi);
  spiReleaseBus(spi);
  led(14, 0x1f);

  if (toRead < bufferSize) ((char *)buf)[toRead] = '\0';
  devp->rxAvailable -= toRead;
  if (!devp->rxAvailable) {
    devp->rxEmpty = true;
    rfm69SetMode(devp, RFM69_RF_OPMODE_RECEIVER);
  }

  /* sprintf(buffer, "read %d chars out of %d, %d left, empty=%d: \"%s\"\n", */
  /* 	  toRead, available, devp->rxAvailable, devp->rxEmpty, (char *)(buf + 4)); */
  /* print(buffer); */
}

uint8_t rfm69ReadRSSI(RFM69Driver *devp) {
  return rfm69ReadReg(devp, RFM69_REG_RSSIVALUE);
}

void rfm69ClearFIFO(RFM69Driver *devp) {
  rfm69WriteReg(devp, RFM69_REG_IRQFLAGS2, rfm69ReadReg(devp, RFM69_REG_IRQFLAGS2) | RFM69_RF_IRQFLAGS2_FIFOOVERRUN);
  devp->rxAvailable = 0;
  devp->rxEmpty = true;
}

void rfm69Send(RFM69Driver *devp, uint8_t bufferSize, const void *buf) {
  rfm69SetMode(devp, RFM69_RF_OPMODE_STANDBY); /* So as not to be disturbed by arriving packet */
  rfm69ClearFIFO(devp);
  /* avoid RX deadlocks */
  rfm69WriteReg(devp, RFM69_REG_PACKETCONFIG2,
		(rfm69ReadReg(devp, RFM69_REG_PACKETCONFIG2) & 0xFB) | RFM69_RF_PACKET2_RXRESTART);

  rfm69WriteReg(devp, RFM69_REG_FIFO, bufferSize-1); /* Write length byte to FIFO */
  rfm69SpiSend(devp, bufferSize, buf);               /* Write everything else to FIFO */

  rfm69SetMode(devp, RFM69_RF_OPMODE_TRANSMITTER); /* Launch transmission */
  waitForCompletion(devp);

  rfm69SetMode(devp, RFM69_RF_OPMODE_RECEIVER);
}
