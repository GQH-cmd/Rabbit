#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace gateway {
struct Config {
  uint32_t version = 1U;
  char ssid[33] = {};
  char wifiPassword[64] = {};
  char host[128] = {};
  char username[65] = {};
  char password[129] = {};
  char device[40] = {};
  uint16_t port = 1883U;
  uint32_t checksum = 0U;
};

static inline uint32_t configHash(const Config &config)
{
  uint32_t hash = 2166136261U;
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&config);
  for (size_t index = 0U; index < offsetof(Config, checksum); ++index) hash = (hash ^ bytes[index]) * 16777619U;
  return hash;
}
static inline void seal(Config &config) { config.checksum = configHash(config); }
static inline bool identifier(const char *value)
{
  if (!value || !*value) return false;
  for (; *value; ++value) if (!((*value >= 'a' && *value <= 'z') || (*value >= 'A' && *value <= 'Z') || (*value >= '0' && *value <= '9') || *value == '-' || *value == '_')) return false;
  return true;
}
static inline bool validHost(const char *value)
{
  if (!value || !*value) return false;
  for (; *value; ++value) if (!((*value >= 'a' && *value <= 'z') || (*value >= 'A' && *value <= 'Z') || (*value >= '0' && *value <= '9') || *value == '-' || *value == '.')) return false;
  return true;
}
template <size_t Size> static inline bool terminated(const char (&text)[Size]) { return memchr(text, 0, Size) != nullptr; }
static inline bool validConfig(const Config &config)
{
  if (config.version != 1U || config.checksum != configHash(config) || !config.port) return false;
  if (!terminated(config.ssid) || !terminated(config.wifiPassword) || !terminated(config.host) || !terminated(config.username) || !terminated(config.password) || !terminated(config.device)) return false;
  const size_t length = strlen(config.wifiPassword);
  return config.ssid[0] && (length == 0U || (length >= 8U && length <= 63U)) && validHost(config.host) && identifier(config.device);
}
static inline bool parsePort(const char *text, uint16_t &port)
{
  if (!text || !*text) return false;
  uint32_t value = 0U;
  for (; *text; ++text) { if (*text < '0' || *text > '9') return false; value = value * 10U + static_cast<uint32_t>(*text - '0'); if (value > 65535U) return false; }
  if (!value) return false;
  port = static_cast<uint16_t>(value);
  return true;
}
struct Command { char text[29] = {}; char request[48] = {}; char session[33] = {}; };
static inline bool space(char value) { return value == ' ' || value == '\t' || value == '\r' || value == '\n'; }
static inline bool parseCommand(const uint8_t *data, size_t length, Command &out)
{
  out = Command{};
  if (!data || !length || length > 256U || memchr(data, 0, length)) return false;
  size_t pos = 0U; unsigned seen = 0U;
  const auto whitespace = [&]() { while (pos < length && space(static_cast<char>(data[pos]))) ++pos; };
  const auto string = [&](char *output, size_t capacity) -> bool {
    if (pos >= length || data[pos++] != '"') return false;
    size_t used = 0U;
    while (pos < length && data[pos] != '"') { const uint8_t value = data[pos++]; if (value < 32U || value > 126U || value == '\\' || used + 1U >= capacity) return false; output[used++] = static_cast<char>(value); }
    if (pos >= length) return false;
    ++pos; output[used] = 0; return used != 0U;
  };
  whitespace(); if (pos >= length || data[pos++] != '{') return false; whitespace();
  for (;;) {
    char key[16] = {}; if (!string(key, sizeof(key))) return false; whitespace();
    if (pos >= length || data[pos++] != ':') return false; whitespace();
    char *destination = nullptr; size_t capacity = 0U; unsigned bit = 0U;
    if (!strcmp(key, "command")) { destination = out.text; capacity = sizeof(out.text); bit = 1U; }
    else if (!strcmp(key, "request_id")) { destination = out.request; capacity = sizeof(out.request); bit = 2U; }
    else if (!strcmp(key, "session_id")) { destination = out.session; capacity = sizeof(out.session); bit = 4U; }
    else return false;
    if ((seen & bit) || !string(destination, capacity)) return false;
    seen |= bit; whitespace(); if (pos >= length) return false;
    const uint8_t separator = data[pos++]; if (separator == '}') break; if (separator != ',') return false; whitespace();
  }
  whitespace(); return pos == length && (seen & 1U) && (!(seen & 2U) || identifier(out.request)) && (!(seen & 4U) || identifier(out.session));
}
static inline bool isStop(const char *command) { return !strcmp(command, "STOP") || !strcmp(command, "DISARM"); }
static inline bool sessionAllowed(const Command &command, const char *current) { return isStop(command.text) || !strcmp(command.text, "PING") || (current && *current && command.session[0] && !strcmp(command.session, current)); }
static inline bool remoteAllowed(const char *command) { return isStop(command) || !strcmp(command, "PING") || !strcmp(command, "ARM") || !strcmp(command, "KEEPALIVE") || !strcmp(command, "FEED") || !strcmp(command, "CLEAN") || !strcmp(command, "RESET_FAULT"); }
struct Lease {
  bool armed = false, remote = false; uint32_t epoch = 0U, last = 0U;
  void arm(bool fromNetwork, uint32_t connection, uint32_t now) { armed = true; remote = fromNetwork; epoch = connection; last = now; }
  void renew(uint32_t now) { last = now; }
  void lock() { armed = false; remote = false; }
  bool expired(uint32_t now, bool online, uint32_t connection) const { return armed && remote && (!online || connection != epoch || static_cast<uint32_t>(now - last) >= 5000U); }
};
} // namespace gateway
