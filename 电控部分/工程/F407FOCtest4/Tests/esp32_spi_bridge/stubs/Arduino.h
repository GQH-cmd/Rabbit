#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <deque>
#include <vector>
#include <utility>
#define CONFIG_IDF_TARGET_ESP32S3 1
#define OUTPUT 1
#define INPUT 0
#define HIGH 1
#define LOW 0
namespace fake {
inline uint32_t now = 0;
inline uint32_t cpu = 240;
inline int cs = HIGH;
inline bool beginOk = true;
inline std::vector<std::vector<uint8_t>> frames;
inline std::vector<uint32_t> times;
inline std::vector<std::pair<int, int>> writes;
inline bool framingOk = true;
inline uint32_t spiHz = 0;
inline int spiMode = -1;
inline int pins[4] = {};
}
// String/readStringUntil exist here only to run regressions on the OLD sketch.
class String {
  std::string s;
public:
  String(std::string v = "") : s(v) {}
  String(const char* v) : s(v) {}
  void trim() {
    auto a = s.find_first_not_of(" \r\n\t");
    if (a == std::string::npos) { s.clear(); return; }
    s = s.substr(a, s.find_last_not_of(" \r\n\t") - a + 1);
  }
  size_t length() const { return s.size(); }
  const char* c_str() const { return s.c_str(); }
};
class FakeSerial {
public:
  std::deque<unsigned char> input;
  std::string output;
  void begin(uint32_t) {}
  void setTimeout(uint32_t) {}
  int available() { return static_cast<int>(input.size()); }
  int read() { if (input.empty()) return -1; auto c = input.front(); input.pop_front(); return c; }
  void feed(const std::string& s) { for (auto c : s) input.push_back(static_cast<unsigned char>(c)); }
  void print(const char* s) { output += s; }
  void println(const char* s = "") { output += s; output += '\n'; }
  void printf(const char* format, ...) {
    char buf[2048]; va_list args; va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args); va_end(args); output += buf;
  }
  String readStringUntil(char delimiter) {
    std::string s; while (available()) { char c = static_cast<char>(read()); if (c == delimiter) break; s += c; }
    return String(s);
  }
};
inline FakeSerial Serial;
inline uint32_t millis() { return fake::now; }
inline void delay(uint32_t n) { fake::now += n; }
inline void delayMicroseconds(uint32_t) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int level) {
  fake::writes.push_back({pin, level}); if (pin == 15) fake::cs = level;
}
inline bool setCpuFrequencyMhz(uint32_t n) { fake::cpu = n; return true; }
inline uint32_t getCpuFrequencyMhz() { return fake::cpu; }
struct FakeESP {
  uint32_t getFlashChipSize() const { return 16 * 1024 * 1024; }
  uint32_t getPsramSize() const { return 8 * 1024 * 1024; }
  uint32_t getFreeHeap() const { return 200000; }
};
inline FakeESP ESP;
