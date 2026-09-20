#pragma once
#include "Arduino.h"
#define FSPI 0
#define MSBFIRST 1
#define SPI_MODE0 0
struct SPISettings {
  uint32_t hz; int order; int mode;
  SPISettings(uint32_t h, int o, int m) : hz(h), order(o), mode(m) {}
};
namespace fake { inline std::deque<std::vector<uint8_t>> replies; }
class SPIClass {
  bool transaction = false;
public:
  explicit SPIClass(int) {}
  bool begin(int sck, int miso, int mosi, int cs) {
    fake::pins[0] = sck; fake::pins[1] = miso; fake::pins[2] = mosi; fake::pins[3] = cs;
    return fake::beginOk;
  }
  void beginTransaction(SPISettings s) {
    transaction = true; fake::spiHz = s.hz; fake::spiMode = s.mode;
  }
  void transferBytes(const uint8_t* tx, uint8_t* rx, uint32_t n) {
    fake::framingOk &= transaction && fake::cs == LOW;
    fake::frames.emplace_back(tx, tx + n); fake::times.push_back(millis());
    if (rx) {
      memset(rx,0xff,n);
      if(!fake::replies.empty()) {auto r=fake::replies.front();fake::replies.pop_front();memcpy(rx,r.data(),r.size()<n?r.size():n);}
    } // floating MISO must NEVER count as an ACK.
  }
  void endTransaction() { fake::framingOk &= fake::cs == HIGH; transaction = false; }
};
