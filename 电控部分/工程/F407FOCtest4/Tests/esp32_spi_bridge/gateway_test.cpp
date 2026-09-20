#include <cstdio>
#include <cstring>
#include "../../build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned/GatewayValidation.h"
static int count = 0;
#define CHECK(x) do { ++count; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
  gateway::Command c{};
  const char* good = " {\"request_id\":\"cmd-1\",\"command\":\"v30,o0\",\"session_id\":\"abc123\"} \n";
  CHECK(gateway::parseCommand(good, std::strlen(good), c));
  CHECK(std::strcmp(c.text, "v30,o0") == 0);
  const char* bad[] = {"{\"command\":\"ARM\"", "{\"command\":\"ARM\"}garbage", "{\"command\":\"ARM\",}",
    "{\"command\":\"STOP\",\"command\":\"ARM\"}", "{\"other\":\"ARM\"}", "{\"command\":null}",
    "{\"command\":\"AR\\u004d\"}", "{\"command\":\"ARM\n\"}", "{}", "ARM", "{\"command\":\"ARM\",\"request_id\":\"a/b\"}"};
  for (auto s : bad) CHECK(!gateway::parseCommand(s, std::strlen(s), c));
  const char embedded[] = "{\"command\":\"ARM\"}\0garbage";
  CHECK(!gateway::parseCommand(embedded, sizeof(embedded)-1, c));
  uint16_t port=0;
  CHECK(gateway::parsePort("1883", port) && port==1883);
  CHECK(!gateway::parsePort("1883abc", port));
  CHECK(!gateway::parsePort("65536", port));
  CHECK(!gateway::parsePort("0", port));
  CHECK(!gateway::parsePort("-1", port));
  CHECK(gateway::validHost("mqtt.example.com"));
  CHECK(!gateway::validHost("mqtt://example.com"));
  CHECK(!gateway::validHost("a b"));
  CHECK(!gateway::remoteAllowed("FACTORY_RESET"));
  CHECK(!gateway::remoteAllowed("CONFIG"));
  CHECK(!gateway::remoteAllowed("vnan,o0"));
  CHECK(gateway::remoteAllowed("v30,o0"));
  gateway::Lease lease{};
  CHECK(!lease.armed);
  lease.arm(true, 7, 100);
  CHECK(!lease.expired(5099, true, 7));
  CHECK(lease.expired(5100, true, 7));
  CHECK(lease.expired(101, false, 7));
  CHECK(lease.expired(101, true, 8));
  lease.renew(1000);
  CHECK(!lease.expired(5500, true, 7));
  lease.lock(); CHECK(!lease.armed);
  lease.arm(true, 9, 0xFFFFFF00U);
  CHECK(!lease.expired(0x00000010U, true, 9));
  CHECK(lease.expired(0x00001400U, true, 9));
  gateway::Config cfg{};
  std::strcpy(cfg.ssid,"test"); std::strcpy(cfg.host,"192.168.1.2"); std::strcpy(cfg.device,"rabbit-foc-001"); cfg.port=1883;
  gateway::seal(cfg); CHECK(gateway::validConfig(cfg));
  cfg.host[0]='8'; CHECK(!gateway::validConfig(cfg));
  gateway::seal(cfg); CHECK(gateway::validConfig(cfg));
  std::memset(cfg.ssid,'A',sizeof(cfg.ssid)); gateway::seal(cfg); CHECK(!gateway::validConfig(cfg));
  std::printf("PASS %d gateway checks\n",count);
}
