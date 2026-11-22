# ESP32-S3 MQTT gateway

This sketch keeps the MQTT contract from the ESP32/FOC gateway while using the
YD-ESP32-S3 board wiring and the STM32 SPI2-slave interface. It does not send
FOC-specific motor commands: it forwards only versioned generic task IDs.

## Wiring

| ESP32-S3 | STM32F407 | Function |
| --- | --- | --- |
| GPIO15 | PB12 | CS/NSS |
| GPIO13 | PB13 | SCK |
| GPIO12 | PB15 | MOSI |
| GPIO14 | PB14 | MISO |
| GND | GND | Common ground |

The pump remains on STM32 PE2. ESP32 GPIO4/5 are not used for relays in this
firmware. GPIO16 remains available for a future READY/IRQ signal.

## MQTT contract

Topics are `rabbit/<device_id>/cmd`, `/ack`, and `/status`. Commands use the
same JSON fields as the FOC gateway: `command`, optional `request_id`, and
`session_id` for all commands except `PING` and STOP. MQTT keepalive is 10 s,
the application lease is 5 s, and status is retained once per second. Commands
must not be retained. ACK publishes use QoS 0, matching the reference gateway.

The task link is protocol v3, fixed 64-byte frames with CRC16. MQTT commands
`PING`, `ARM`, `FEED`, `CLEAN`, `STOP`, and `RESET_FAULT` map to task IDs;
`KEEPALIVE` is local to the ESP32 lease. The STM32 reports state, task, self-
test, and fault information. Production firmware rejects `FEED` and `CLEAN`
until the serial motor protocol is known; the dedicated Simulation firmware runs
safe virtual task flows instead.

## Local integration test

The USB serial monitor runs at 115200 baud and can act as a website/MQTT
substitute while the network is unavailable. It uses the same task-to-SPI path
as MQTT, so it does not bypass the STM32 link:

```text
STATUS
PING
ARM
FEED
CLEAN
STOP
RESET_FAULT
```

At boot the ESP32 prints `LOCAL_CONSOLE_READY session_id=...`. To exercise the
same JSON parser and session check used by a website, send one line such as:

```text
WEB {"command":"PING","request_id":"local-1","session_id":"copy-the-printed-session-id"}
```

The following commands are serial-only and are accepted only by an STM32 built
with the `Simulation` CMake preset. They are always rejected by production
firmware and cannot arrive through MQTT:

```text
SIM FAIL_NEXT
SIM TIMEOUT_NEXT
SIM CLEAR_FAULT
```

`Simulation` runs the FEED/CLEAN task sequence against the six debounced
position inputs: horizontal end switch, lower position switch, timed virtual
pump phase(s), and the matching upper switch. It reports requested motor
directions and pump channel through the STM32 application runtime for
diagnostics, but it never drives a motor or pump. PA1 is only a virtual pump
indicator; PE2 remains off. Each limit wait times out after 30 s. A simulated
failure or timeout first ends in `FAULT`; use `RESET_FAULT` afterward to return
to `IDLE`.

FEED uses the left, left-down and left-up switches and a 2 s virtual milk-pump
phase. CLEAN uses the right, right-down and right-up switches, with a 3 s
cleaner phase followed by a 5 s water phase. Production firmware continues to
reject FEED/CLEAN until the separate motor serial and pump drivers are
implemented.

## Build

Open the `ESP32/` sketch directory (whose Arduino entry file is `ESP32.ino`)
in Arduino IDE with an ESP32-S3 board package and PubSubClient installed. The
gateway implementation remains in `esp32_gateway.cpp`. Network settings are
not compiled into the sketch:
on an unconfigured device it opens a `Rabbit-Setup-...` AP and stores the
submitted settings in the established `rabbitnet/config` NVS record. The serial
monitor is 115200 baud. `CONFIG` reopens that page; `LOCAL` pauses new Wi-Fi /
MQTT reconnect attempts while retaining an existing online MQTT link only for
remote `STOP`.

The configuration-save path logs `NET_SAVE_OK` after an NVS commit and exact
read-back, or a non-secret `NET_SAVE_FAIL stage=...` diagnostic. Existing NVS
configuration written by the original gateway is retained and remains readable.
