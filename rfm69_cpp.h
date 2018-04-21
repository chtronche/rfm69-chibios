#ifndef _RFM69_CPP_H_
#define _RFM69_CPP_H_

// @file   rfm69_cpp.h
// @brief  RFM69 radio module header -- C++ interface

class RFM69 {

  typedef uint8_t freq_t;

 public:
 RFM69(SPIDriver *spip, const SPIConfig *spicfg, ioportid_t ssport, uint16_t sspad, freq_t freq)
   :_spip(spip), _spicfg(spicfg), _ssport(ssport), _sspad(ssport) {
  }

  void send(uint16_t toAddress, const void *buffer, uint8_t bufferSize, bool requestACK=false);
  uint8_t readReg(uint8_t addr);
  void writeReg(uint8_t addr, uint8_t val);
  
};

#endif // _RFM69_CPP_H_
