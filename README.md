# ESP32-C6 IKEA Zigbee wake light

An ESP32-C6 controller for an IKEA Zigbee bulb. The ESP32 creates the Zigbee
network, binds to one bulb, and serves a small control page over Wi-Fi.

The controller supports:

- on/off, brightness, and white-temperature controls;
- automatic Zigbee pairing until a bulb is bound;
- a daily wake-light alarm stored on the ESP32;
- an optional alarm time supplied by another device on the local network;
- configurable pre-wake, bright, calm, and automatic-off stages;
- a BOOT-button shortcut that turns the bulb off.

## Hardware

- ESP32-C6 development board with 4 MB flash
- IKEA Zigbee bulb with dimming and adjustable white temperature
- USB cable for flashing

The sketch assumes the board's NeoPixel is on GPIO 8 and its BOOT button is on
GPIO 9. Change `PIXEL_PIN` and `OFF_BUTTON_PIN` near the top of the sketch if
your board uses different pins.

## Software

- Arduino IDE 2
- Espressif Arduino core for ESP32, version 3.3 or newer
- Adafruit NeoPixel library

## Configure and flash

1. Open
   `Zigbee_wifi_webserver/Zigbee_wifi_webserver.ino` in the Arduino IDE.
2. Copy `Zigbee_wifi_webserver/secrets.example.h` to
   `Zigbee_wifi_webserver/secrets.h`.
3. Put your 2.4 GHz Wi-Fi name and password in `secrets.h`.
4. Select `ESP32C6 Dev Module` under **Tools > Board**.
5. Select `Enabled` under **Tools > USB CDC On Boot**.
6. Select `Zigbee ZCZR (coordinator/router)` under **Tools > Zigbee mode**.
7. Select `Custom` under **Tools > Partition Scheme**. The included
   `partitions.csv` keeps Espressif's Zigbee storage partitions and provides
   enough room for the firmware.
8. Upload the sketch and open the serial monitor at 115200 baud.

`secrets.h` is ignored by Git and must not be committed.

The custom 4 MB layout has one large application partition and no
over-the-air update slot. Upload firmware updates over USB.

## Pair a bulb

1. Keep the bulb close to the ESP32-C6.
2. Reset the bulb by switching its power off and on six times, ending with the
   bulb powered on.
3. Wait while the ESP32 keeps the Zigbee network open for pairing.
4. Open the address printed in the serial monitor, or try
   `http://wake-light.local/`.

The onboard status light indicates:

- blinking blue: connecting to Wi-Fi;
- blinking purple: starting Zigbee;
- breathing amber: waiting for a bulb;
- solid green: bulb paired;
- fast blinking red: startup failed.

If an old Zigbee network prevents pairing, erase the ESP32 flash from the
Arduino IDE and upload the sketch again.

## Wake-light behavior

The built-in daily alarm uses the `Europe/Copenhagen` timezone and obtains the
current time from an NTP server. It will not fire until the clock has
synchronized.

The default routine starts at full brightness and 4000 K, changes to 35% at
2700 K after 10 minutes, and turns off after 20 minutes. These values can be
changed on the control page.

Keep mains power connected to the bulb. Turn it off through the control page or
with a short press of the ESP32-C6 BOOT button.

## Local HTTP endpoints

The web interface uses the following endpoints:

| Method | Path | Purpose |
| --- | --- | --- |
| `GET` | `/` | Open the control page |
| `GET` | `/status` | Read controller state as JSON |
| `POST` | `/on` | Turn the bulb on |
| `POST` | `/off` | Turn the bulb off |
| `POST` | `/set?brightness=50&temperature=3500` | Set brightness and white temperature |
| `POST` | `/pair` | Open pairing for 180 seconds |
| `POST` | `/alarm?enabled=1&hour=06&minute=30` | Save the daily alarm |
| `POST` | `/wake` | Start the wake-light routine immediately |
| `POST` | `/phone-alarm?epoch=1785219000` | Store the next external alarm as a Unix timestamp |
| `POST` | `/presence` | Record that the external alarm device is reachable |

The controller has no login or transport encryption. Use it only on a trusted
local network and do not expose port 80 to the internet.

## Serial commands

The serial monitor accepts `on`, `off`, `toggle`, `pair`, `devices`, and
`help`.
