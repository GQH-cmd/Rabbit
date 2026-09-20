#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
// 显式声明 WebServer 使用的哈希依赖，避免依赖扫描遗漏。
#include <SHA1Builder.h>
#include <MD5Builder.h>
#include <WebServer.h>
#include "GatewayApp.h"

void setup() { app::begin(); }
void loop() { app::tick(); }
