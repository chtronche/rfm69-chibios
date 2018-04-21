/**********************************************************************************
 Driver definition for HopeRF RFM69W/RFM69HW/RFM69CW/RFM69HCW, Semtech SX1231/1231H
 **********************************************************************************
 Copyright LowPowerLab LLC 2018, https://www.LowPowerLab.com/contact
 Adapted for ChibiOS by Ch.Tronche, ch@tronche.com, (c) 2018
 **********************************************************************************
 License
 **********************************************************************************
 This program is free software; you can redistribute it 
 and/or modify it under the terms of the GNU General    
 Public License as published by the Free Software       
 Foundation; either version 3 of the License, or        
 (at your option) any later version.                    
                                                        
 This program is distributed in the hope that it will   
 be useful, but WITHOUT ANY WARRANTY; without even the  
 implied warranty of MERCHANTABILITY or FITNESS FOR A   
 PARTICULAR PURPOSE. See the GNU General Public        
 License for more details.                              
                                                        
 Licence can be viewed at                               
 http://www.gnu.org/licenses/gpl-3.0.txt

 Please maintain this license information along with authorship
 and copyright notices in any redistribution of this code
**********************************************************************************/
/*
 * @file	rfm69.h
 * @brief	RFM69 radio module header
 */

#ifndef _RFM69_H_
#define _RFM69_H_

#include "hal.h"

// available frequency bands
// non trivial values to avoid misconfiguration
typedef enum { 
  RFM69_315MHZ = 31, 
  RFM69_433MHZ = 43, 
  RFM69_868MHZ = 86, 
  RFM69_915MHZ = 91
} rfm69_frequency_t;

typedef struct {
  SPIDriver         *spip;
  const SPIConfig   *spiConfig;
  ioportid_t        ssport;
  uint16_t          sspad;
  rfm69_frequency_t frequency;
  /* End of mandatory fields */
} RFM69Config;

typedef enum {
  rfm69_status_ok,             /* 0 for faster testing */
  rfm69_status_uninitialized,
  rfm69_status_stop,
  rfm69_status_bad_frequency,
} rfm69_status_t;

typedef struct {
  RFM69Config *config;
  rfm69_status_t status;
} RFM69Driver;

#define RF69_MAX_DATA_LEN       61 // to take advantage of the built in AES/CRC we want to limit the frame size to the internal FIFO size (66 bytes - 3 bytes overhead - 2 bytes crc)
#define RF69_CSMA_LIMIT              -90 // upper RX signal sensitivity threshold in dBm for carrier sense access
#define RF69_MODE_SLEEP         0 // XTAL OFF
#define RF69_MODE_STANDBY       1 // XTAL ON
#define RF69_MODE_SYNTH         2 // PLL ON
#define RF69_MODE_RX            3 // RX MODE
#define RF69_MODE_TX            4 // TX MODE

#define RF69_COURSE_TEMP_COEF    -90 // puts the temperature reading in the ballpark, user can fine tune the returned value
#define RF69_BROADCAST_ADDR 255
#define RF69_CSMA_LIMIT_MS 1000
#define RF69_TX_LIMIT_MS   1000
#define RF69_FSTEP  61.03515625 // == FXOSC / 2^19 = 32MHz / 2^19 (p13 in datasheet)

// TWS: define CTLbyte bits
#define RFM69_CTL_SENDACK   0x80
#define RFM69_CTL_REQACK    0x40

void rfm69ObjectInit(RFM69Driver *);
void rfm69Start(RFM69Driver *, RFM69Config *);
void rfm69Stop(RFM69Driver *);

void rfm69Send(RFM69Driver *, uint8_t toAddress, const void *buffer, uint8_t bufferSize, bool requestACK);
void rfm69SendWithRetry(RFM69Driver *, uint8_t toAddress, const void *buffer, uint8_t bufferSize, uint8_t retries, uint8_t retryWaitTime);

void rfm69Read(RFM69Driver *, void *buffer, uint8_t bufferSize);
void rfm69ReadWithTimeout(void);

void rfm69Reset(ioportid_t resetIOPort, uint16_t resetPad);
void rfm69SetFrequency(RFM69Driver *, rfm69_frequency_t);

void rfm69RcCalibration(RFM69Driver *);
uint8_t rfm69ReadTemperature(RFM69Driver *, uint8_t calFactor); // get CMOS temperature (8bit)

/* Direct register access primitives */

uint8_t rfm69ReadReg(RFM69Driver *, uint8_t addr);
void rfm69WriteReg(RFM69Driver *, uint8_t addr, uint8_t values);

#endif /* _RFM69_H_ */
