#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

// 与 STM32 的 App_SPI3_CommandTask() 保持线缆协议兼容；旧版 SPI 没有 ACK。
namespace foc {
constexpr size_t kFrameSize = 32;
constexpr size_t kMaxPayload = 28;
constexpr uint8_t kSof = 0xA5;
inline uint8_t crc8(const uint8_t* bytes, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = static_cast<uint8_t>((crc & 0x80U) ? ((crc << 1U) ^ 0x07U) : (crc << 1U));
  }
  return crc;
}
inline bool buildFrame(const char* payload, uint8_t sequence, uint8_t (&frame)[kFrameSize]) {
  memset(frame, 0, kFrameSize);
  if (payload == nullptr) return false;
  const size_t length = strlen(payload);
  if (length == 0 || length > kMaxPayload) return false;
  frame[0] = kSof; frame[1] = sequence; frame[2] = static_cast<uint8_t>(length);
  memcpy(frame + 3, payload, length);
  frame[3 + length] = crc8(frame, 3 + length);
  return true;
}
inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
// 只接受十进制格式：不允许 NaN/Inf、十六进制、尾部垃圾字符或省略电机模式。
inline bool parseMotorField(const char* field, size_t length, char& mode, float& value) {
  if (length < 2 || length > kMaxPayload) return false;
  mode = field[0];
  if (mode != 'o' && mode != 'c' && mode != 'v' && mode != 'p') return false;
  size_t i = 1;
  if (field[i] == '+' || field[i] == '-') ++i;
  size_t digits = 0;
  while (i < length && isDigit(field[i])) { ++i; ++digits; }
  if (i < length && field[i] == '.') {
    ++i;
    while (i < length && isDigit(field[i])) { ++i; ++digits; }
  }
  if (digits == 0) return false;
  if (i < length && (field[i] == 'e' || field[i] == 'E')) {
    ++i;
    if (i < length && (field[i] == '+' || field[i] == '-')) ++i;
    const size_t exponentStart = i;
    while (i < length && isDigit(field[i])) ++i;
    if (i == exponentStart) return false;
  }
  if (i != length) return false;
  char number[kMaxPayload + 1] = {};
  memcpy(number, field + 1, length - 1);
  char* end = nullptr;
  errno = 0;
  value = strtof(number, &end);
  return errno != ERANGE && end == number + length - 1 && isfinite(value);
}
inline bool validMotorCommand(const char* command, bool& zeroOpenLoop) {
  zeroOpenLoop = false;
  if (command == nullptr) return false;
  const size_t length = strlen(command);
  if (length == 0 || length > kMaxPayload) return false;
  const char* comma = strchr(command, ',');
  if (!comma || strchr(comma + 1, ',')) return false;
  char mode0 = 0, mode1 = 0;
  float value0 = 0, value1 = 0;
  if (!parseMotorField(command, static_cast<size_t>(comma - command), mode0, value0) ||
      !parseMotorField(comma + 1, strlen(comma + 1), mode1, value1)) return false;
  zeroOpenLoop = mode0 == 'o' && mode1 == 'o' && value0 == 0.0f && value1 == 0.0f;
  return true;
}
} // foc 命名空间
