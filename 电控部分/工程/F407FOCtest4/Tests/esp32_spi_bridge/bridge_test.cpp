#include <functional>
#include <stdexcept>
#include <iostream>
#include "../../build-gcc/esp32_spi/esp32_spi_bridge/esp32_spi_bridge.ino"

#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (0)
static void pump() { for (int i = 0; i < 20; ++i) loop(); }
static void feed(const std::string& s) { Serial.feed(s); pump(); }
static void reset(bool spiOk = true) {
  fake::now = 0; fake::beginOk = spiOk; fake::cs = HIGH;
  fake::frames.clear(); fake::times.clear(); fake::writes.clear();
  fake::framingOk = true; Serial.input.clear(); Serial.output.clear();
  setup();
}
static void clearTraffic() { fake::frames.clear(); fake::times.clear(); Serial.output.clear(); }
static std::string payload(size_t n = 0) {
  const auto& f = fake::frames.at(n);
  CHECK(f.size() == 32); CHECK(f[2] > 0 && f[2] <= 28);
  return std::string(f.begin() + 3, f.begin() + 3 + f[2]);
}
// Independent bitwise polynomial long division (not the production CRC loop).
static uint8_t referenceCrc(const uint8_t* data, size_t n) {
  unsigned remainder = 0;
  for (size_t i = 0; i < n; ++i) {
    remainder ^= static_cast<unsigned>(data[i]) << 8;
    for (int b = 0; b < 8; ++b) {
      if (remainder & 0x8000U) remainder ^= 0x8380U;
      remainder = (remainder << 1) & 0xFFFFU;
    }
  }
  return static_cast<uint8_t>(remainder >> 8);
}
static void checkFrame(const std::vector<uint8_t>& f) {
  CHECK(f.size() == 32 && f[0] == 0xA5);
  size_t n = f[2]; CHECK(n > 0 && n <= 28);
  CHECK(f[3 + n] == referenceCrc(f.data(), 3 + n));
  for (size_t i = 4 + n; i < f.size(); ++i) CHECK(f[i] == 0);
}
int main() {
  int passed = 0, failed = 0;
  auto test = [&](const char* name, const std::function<void()>& fn) {
    try { fn(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) { ++failed; std::cout << "FAIL " << name << ": " << e.what() << '\n'; }
  };
  test("reference CRC check value", [] {
    const auto* s = reinterpret_cast<const uint8_t*>("123456789");
    CHECK(referenceCrc(s, 9) == 0xF4);
  });
  test("boot emits no SPI clocks or motor commands", [] {
    reset(); CHECK(fake::frames.empty()); CHECK(fake::cs == HIGH);
    CHECK(fake::pins[0] == 13 && fake::pins[1] == 14 && fake::pins[2] == 12 && fake::pins[3] == 15);
  });
  test("PING is a valid fixed 32-byte frame, not an ACK", [] {
    reset(); clearTraffic(); feed("PING\n"); CHECK(fake::frames.size() == 1);
    CHECK(payload() == "PING"); checkFrame(fake::frames[0]);
    CHECK(fake::spiMode == SPI_MODE0 && fake::spiHz == 100000);
    CHECK(fake::framingOk); CHECK(Serial.output.find("ACK=UNAVAILABLE") != std::string::npos);
  });
  test("motor commands require local ARM", [] {
    reset(); clearTraffic(); feed("v30,o0\n"); CHECK(fake::frames.empty());
    CHECK(Serial.output.find("NOT_ARMED") != std::string::npos);
  });
  test("ARM and status are local, do not leak to motor parser", [] {
    reset(); clearTraffic(); feed("HELP\nSTATUS\nARM\n"); CHECK(fake::frames.empty());
  });
  test("armed motor command preserves both explicit modes", [] {
    reset(); clearTraffic(); feed("ARM\nv30,o0\n"); CHECK(fake::frames.size() == 1);
    CHECK(payload() == "v30,o0"); checkFrame(fake::frames[0]);
  });
  test("reject malformed commands even when armed", [] {
    const char* bad[] = {"v30,0", "v30", "v30,", ",o0", "v30,o0,o0", "x1,o0", "V30,o0",
      "vnan,o0", "vNaN,o0", "vinf,o0", "v1e99,o0", "o0x10,o0", "v1junk,o0", "o.,o0",
      "o+.,o0", "o--1,o0", "v1, o0", "start", "reset", "PING,o0"};
    for (const auto* s : bad) {
      reset(); clearTraffic(); feed("ARM\n"); feed(std::string(s) + "\n"); CHECK(fake::frames.empty());
    }
  });
  test("valid decimal modes and signed targets", [] {
    const char* good[] = {"o0.2,o0", "c-0.1,c0", "v+30,o0", "p1.25,p-2", "v1e2,v-1e2"};
    for (const auto* s : good) {
      reset(); clearTraffic(); feed("ARM\n"); feed(std::string(s) + "\n");
      CHECK(fake::frames.size() == 1); CHECK(payload() == s); checkFrame(fake::frames[0]);
    }
  });
  test("STOP always sends zero-open-loop and locks subsequent motion", [] {
    reset(); clearTraffic(); feed("ARM\nSTOP\nv30,o0\n");
    CHECK(fake::frames.size() == 1); CHECK(payload() == "o0,o0");
  });
  test("raw zero-open-loop stop allowed without ARM", [] {
    reset(); clearTraffic(); feed("o0,o0\n"); CHECK(fake::frames.size() == 1); CHECK(payload() == "o0,o0");
  });
  test("CR LF and CRLF accepted without duplicate commands", [] {
    reset(); clearTraffic(); feed("PING\rPING\nPING\r\n"); CHECK(fake::frames.size() == 3);
  });
  test("partial serial line never sends by timeout", [] {
    reset(); clearTraffic(); feed("PIN"); CHECK(fake::frames.empty());
    fake::now += 2000; pump(); feed("G\n"); CHECK(fake::frames.empty());
    feed("PING\n"); CHECK(fake::frames.size() == 1);
  });
  test("line overflow discards entire line and tail", [] {
    reset(); clearTraffic(); feed(std::string(120, 'x') + "PING\n"); CHECK(fake::frames.empty());
    feed("PING\n"); CHECK(fake::frames.size() == 1);
  });
  test("NUL and non-ASCII do not hide a command suffix", [] {
    reset(); clearTraffic(); std::string nul("PING\0ignored\n", 13); feed(nul); CHECK(fake::frames.empty());
    feed(std::string("PING") + char(0xFF) + "\n"); CHECK(fake::frames.empty());
    feed("PING\n"); CHECK(fake::frames.size() == 1);
  });
  test("28-byte payload fits CRC at index 31; 29 rejected", [] {
    reset(); clearTraffic(); feed("ARM\n");
    const std::string s = "v" + std::string(23, '0') + ",o0"; CHECK(s.size() == 27);
    feed("v" + std::string(24, '0') + ",o0\n"); CHECK(fake::frames.size() == 1);
    CHECK(fake::frames[0][2] == 28); checkFrame(fake::frames[0]);
    feed("v" + std::string(25, '0') + ",o0\n"); CHECK(fake::frames.size() == 1);
  });
  test("sequence wraps and frame spacing is bounded", [] {
    reset(); clearTraffic();
    for (int i = 0; i < 260; ++i) feed("PING\n");
    CHECK(fake::frames.size() == 260);
    for (size_t i = 0; i < 260; ++i) {
      CHECK(fake::frames[i][1] == static_cast<uint8_t>(i)); checkFrame(fake::frames[i]);
      if (i) CHECK(static_cast<uint32_t>(fake::times[i] - fake::times[i-1]) >= 100);
    }
  });
  test("millis wrap preserves rate limiting", [] {
    reset(); clearTraffic(); fake::now = UINT32_MAX - 50; feed("PING\nPING\n");
    CHECK(fake::frames.size() == 2); CHECK(static_cast<uint32_t>(fake::times[1] - fake::times[0]) >= 100);
  });
  test("SPI init failure blocks transmission", [] {
    reset(false); clearTraffic(); feed("PING\nARM\no0.2,o0\nSTOP\n"); CHECK(fake::frames.empty());
    CHECK(Serial.output.find("SPI_NOT_READY") != std::string::npos);
  });
  test("idle never periodically replays old motor commands", [] {
    reset(); clearTraffic(); feed("ARM\nv30,o0\n"); CHECK(fake::frames.size() == 1);
    for (int i = 0; i < 10; ++i) { fake::now += 5000; pump(); }
    CHECK(fake::frames.size() == 1);
  });
  std::cout << passed << " passed; " << failed << " failed\n";
  return failed ? 1 : 0;
}
