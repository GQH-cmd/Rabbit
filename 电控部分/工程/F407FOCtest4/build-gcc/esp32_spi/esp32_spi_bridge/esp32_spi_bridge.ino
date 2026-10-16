#include <Arduino.h>
#include <SPI.h>
#include "FocSpiProtocol.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select ESP32S3 Dev Module. This wiring is for ESP32-S3, not classic ESP32/C3."
#endif

// ESP32-S3 N16R8 -> STM32 SPI3 (M1 connector), NOT the 2x3 BOOT header.
// GPIO15 -> PA15/CS; GPIO13 -> PC10/SCK; GPIO14 <- PC11/MISO;
// GPIO12 -> PC12/MOSI; GND <-> DGND. Leave both power rails UNCONNECTED.
// COM USB: USB CDC On Boot=Disabled. Native USB: CDC On Boot=Enabled.
static constexpr uint8_t PIN_SPI_CS = 15;
static constexpr uint8_t PIN_SPI_SCK = 13;
static constexpr uint8_t PIN_SPI_MISO = 14;
static constexpr uint8_t PIN_SPI_MOSI = 12;
static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t SPI_CLOCK_HZ = 100000; // slow first for the STM32 IT receiver
static constexpr uint32_t MIN_FRAME_GAP_MS = 100;
static constexpr uint32_t LINE_TIMEOUT_MS = 1500;
static constexpr size_t LINE_CAPACITY = 96;
static constexpr size_t SERIAL_BYTE_BUDGET = 64;

SPIClass focSpi(FSPI);
static bool spiReady = false;
static bool armed = false; // local input guard ONLY; not physical motor state
static bool sentAnyFrame = false;
static uint8_t sequenceNumber = 0;
static uint32_t txCount = 0;
static uint32_t rejectedCount = 0;
static uint32_t lastFrameMs = 0;
static char lineBuffer[LINE_CAPACITY] = {};
static size_t lineLength = 0;
static bool droppingLine = false;
static uint32_t lastByteMs = 0;

static void reject(const char* reason) {
  ++rejectedCount;
  Serial.printf("ERR %s\n", reason);
}
static bool sameWord(const char* text, const char* upper) {
  while (*text && *upper) {
    char c = *text++;
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c != *upper++) return false;
  }
  return *text == '\0' && *upper == '\0';
}
static void printHelp() {
  Serial.println("ESP32-S3 FOC SPI bridge v2 (2026-09-19)");
  Serial.println("HELP / STATUS : local only; PING : harmless link test");
  Serial.println("ARM : allow motor commands; STOP / DISARM : send o0,o0 and lock");
  Serial.println("Motor format: <mode><number>,<mode><number>, e.g. v30,o0");
  Serial.println("Modes: o=open-loop, c=current, v=speed RPM, p=position. Both modes REQUIRED.");
  Serial.println("Max payload 28 ASCII bytes. End each line with CR, LF or CRLF.");
  Serial.println("TX != received/executed. ACK=UNAVAILABLE on legacy FOC SPI firmware.");
  Serial.println("Check FOC USB1/XCOM 115200 for SPI3 PONG or SPI3 OK with matching seq.");
  Serial.println("STOP is a transmitted request, NOT a hardware emergency stop.");
}
static void printStatus() {
  Serial.printf("LOCAL spi_ready=%u armed=%u tx=%lu rejected=%lu cpu_mhz=%lu heap=%lu\n",
                static_cast<unsigned>(spiReady), static_cast<unsigned>(armed),
                static_cast<unsigned long>(txCount), static_cast<unsigned long>(rejectedCount),
                static_cast<unsigned long>(getCpuFrequencyMhz()),
                static_cast<unsigned long>(ESP.getFreeHeap()));
  Serial.printf("SPI3 CS=%u SCK=%u MISO=%u MOSI=%u mode=0 hz=%lu frame=32\n",
                PIN_SPI_CS, PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI,
                static_cast<unsigned long>(SPI_CLOCK_HZ));
  Serial.println("FOC_LINK=NOT_VERIFIED ACK=UNAVAILABLE (use FOC USB1 log)");
}
static bool transmit(const char* command) {
  if (!spiReady) { reject("SPI_NOT_READY"); return false; }
  uint8_t tx[foc::kFrameSize];
  uint8_t rx[foc::kFrameSize] = {};
  if (!foc::buildFrame(command, sequenceNumber, tx)) {
    reject("PAYLOAD_LENGTH_1_TO_28"); return false;
  }
  // STM32 has one RX buffer/ready flag. Pacing reduces risk, but is NOT an ACK.
  if (sentAnyFrame) {
    const uint32_t elapsed = static_cast<uint32_t>(millis() - lastFrameMs);
    if (elapsed < MIN_FRAME_GAP_MS) delay(MIN_FRAME_GAP_MS - elapsed);
  }
  focSpi.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_SPI_CS, LOW);
  delayMicroseconds(10);
  focSpi.transferBytes(tx, rx, foc::kFrameSize);
  delayMicroseconds(10);
  digitalWrite(PIN_SPI_CS, HIGH);
  focSpi.endTransaction();
  lastFrameMs = millis();
  sentAnyFrame = true;
  ++sequenceNumber;
  ++txCount;
  // rx is intentionally NOT interpreted: existing FOC has no SPI TX protocol.
  Serial.printf("SPI TX seq=%u len=%u cmd=%s ACK=UNAVAILABLE\n",
                static_cast<unsigned>(tx[1]), static_cast<unsigned>(tx[2]), command);
  return true;
}
static void processLine(char* text) {
  while (*text == ' ' || *text == '\t') ++text;
  size_t n = strlen(text);
  while (n && (text[n-1] == ' ' || text[n-1] == '\t')) text[--n] = '\0';
  if (!n) return;
  if (sameWord(text, "HELP") || sameWord(text, "?")) { printHelp(); return; }
  if (sameWord(text, "STATUS")) { printStatus(); return; }
  if (sameWord(text, "PING")) { transmit("PING"); return; }
  if (sameWord(text, "ARM")) {
    if (!spiReady) { reject("SPI_NOT_READY"); return; }
    armed = true;
    Serial.println("LOCAL ARMED (not a FOC command; no motor started)");
    return;
  }
  if (sameWord(text, "STOP") || sameWord(text, "DISARM")) {
    armed = false;
    transmit("o0,o0");
    Serial.println("LOCAL LOCKED; confirm actual stop on FOC; no automatic retries");
    return;
  }
  bool zeroOpenLoop = false;
  if (!foc::validMotorCommand(text, zeroOpenLoop)) {
    reject("BAD_COMMAND use both modes, e.g. v30,o0; reject NaN/Inf/junk/len>28");
    return;
  }
  if (zeroOpenLoop) armed = false;
  else if (!armed) { reject("NOT_ARMED send ARM first"); return; }
  transmit(text);
}
static void pollConsole() {
  // Never execute partial input on timeout: discard through the NEXT newline.
  if (lineLength && static_cast<uint32_t>(millis() - lastByteMs) >= LINE_TIMEOUT_MS) {
    lineLength = 0; droppingLine = true;
    reject("LINE_TIMEOUT discarded; send a newline then retry the whole command");
  }
  size_t budget = SERIAL_BYTE_BUDGET;
  while (budget-- && Serial.available()) {
    const int value = Serial.read();
    if (value < 0) break;
    const char c = static_cast<char>(value);
    lastByteMs = millis();
    if (c == '\r' || c == '\n') {
      if (!droppingLine && lineLength) {
        lineBuffer[lineLength] = '\0';
        processLine(lineBuffer);
      }
      lineLength = 0; droppingLine = false;
      continue;
    }
    if (droppingLine) continue;
    if ((value < 32 && c != '\t') || value > 126 || lineLength >= LINE_CAPACITY - 1) {
      reject("BAD_OR_LONG_LINE discarded through newline");
      lineLength = 0; droppingLine = true;
      continue;
    }
    lineBuffer[lineLength++] = c;
  }
}
void setup() {
  // No Wi-Fi/Bluetooth, motor start, auto-PING, or replay of saved commands.
  armed = false; spiReady = false; sentAnyFrame = false;
  sequenceNumber = 0; txCount = 0; rejectedCount = 0; lastFrameMs = 0;
  lineLength = 0; droppingLine = false; lastByteMs = 0;
  Serial.begin(SERIAL_BAUD);
  delay(300); // no infinite wait for a USB serial monitor
  if (!setCpuFrequencyMhz(80)) Serial.println("WARN CPU clock unchanged");
  pinMode(PIN_SPI_CS, OUTPUT);
  digitalWrite(PIN_SPI_CS, HIGH);
  spiReady = focSpi.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
  if (!spiReady) reject("SPI_INIT_FAILED");
  printHelp();
  printStatus();
  Serial.println("READY LOCAL LOCKED. Send PING first; boot sent NO SPI frames.");
}
void loop() {
  pollConsole();
  delay(2); // yield; no busy-spin and no periodic motor commands
}
