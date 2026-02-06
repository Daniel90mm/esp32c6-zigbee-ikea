# Introduction

The motivation was that I wanted to control my light bulb in my dorm room via my PC and phone.

I also wanted to later, if not already implemented, create a kind of "alarm" that wakes me up more easily in the morning by turning on the bulb at the highest color temperature (kelvin) and highest brightness (lumen).

The ESP32 is a C6 model, so it supports Zigbee, a technology that allows communication with sensors via a single device.


# LED - Neopixel
To initialize the LED, you need to include the library and the following code:
```C
#include <Adafruit_NeoPixel.h>      // Doesn't actually have to be this library, but this one works
#define NUMPIXELS 1                 // Number of pixels, only one on the ESP32
#define PIN 8                       // The LED is connected to pin 8
Adafruit_NeoPixel pixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
```


Then you can control the LED with these functions:
```C
pixel.begin();                                  // Required

pixel.setPixelColor(0, pixel.Color(60, 0, 0));  // pixel.setPixelColor(0, ...) sets the color for LED number 0
                                                // pixel.Color(60, 0, 0) sets the color to
                                                // red 60/255, green 0/255 and blue 0/255

pixel.show();                                   // Used to update the LED, i.e. after running
                                                // setPixelColor and clear()

pixel.clear();                                  // Turns off the LED
```

# Wifi
First, you need to import the library:
```C
#include <WiFi.h>
```
Generally, having this function and sequence run in setup() should make everything work:
```C
const char* ssid = "...";
const char* password = "...";

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}
```
#### Wifi Static IP
You might get a random address each time due to DHCP (Dynamic Host Configuration Protocol), so to get the same address every time, use this outside of ``setup()`` and ``loop()``
```C
if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Configuration error")
}
```
Here is an explanation of the parameters, the last two parameters are not required:
```C
// Static IP address, must not be an already used IP address! Preferably between 192.168.1.180-199
IPAddress local_IP(192, 168, 1, 184);

// Gateway IP address, this is the IP address of the connected router
IPAddress gateway(192, 168, 8, 1); // On GLi-net router

// Defines the subnet mask, tells which IPs are local.
// (255, 255, 255, 0) Means only IPs with the same first 3 octets are local, this is the default
// (255, 255, 0, 0) Means the entire 192.168.x.x range is local.
IPAddress subnet(255, 255, 0, 0);

// Not important, can just be this
IPAddress primaryDNS(8, 8, 8, 8);   // Google DNS
IPAddress secondaryDNS(8, 8, 4, 4); // Google DNS
```
#### Wifi Status
The table below explains what the function wifi.status() returns:
```C
WiFi.status()
```
|Value|String|Meaning|
|-|-|-|
|0|`WL_IDLE_STATUS`|Status given when `WiFi.begin()` has been called|
|1|`WL_NO_SSID_AVAIL`|No SSIDs are available|
|2|`WL_SCAN_COMPLETED`|Network scan is complete|
|3|`WL_CONNECTED`|Connected to a network|
|4|`WL_CONNECT_FAILED`|All connection attempts failed|
|5|`WL_CONNECTION_LOST`|Connection lost|
|6|`WL_DISCONNECTED`|Disconnected from network|

#### Wifi Modes
```C
WiFi.mode(WIFI_STA);    // Station mode, ESP32 connects to an access point
WiFi.mode(WIFI_AP);     // Access point mode, devices can connect to the ESP32
WiFi.mode(WIFI_AP_STA); // Access point and station connected to another access point
```
#### Wifi Local IP
This function simply returns the local IP that can be accessed with a browser:
```C
WiFi.localIP(); // returns e.g. 192.168.8.227
```
# Web Server
First, include the library:
```C
#include <WebServer.h>
```
Generally, having this function and sequence run in setup() should make everything work:
```C
WebServer server(80);


// Functions to handle button presses or other actions on root
void Off() {
  outputState = "off";
  // Do something here, blink LED, whatever
  handleRoot();
}

void On() {
  outputState = "on";
  // Do something here, blink LED, whatever
  handleRoot();
}


void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESP32 Web Server</h1>";

  // Show controls
  html += "<p>Neopixel LED state: " + outputState + "</p>";
  if (outputState == "off") {
    html += "<p><a href=\"/on\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/off\"><button class=\"button button2\">OFF</button></a></p>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Initialize neopixel

  // Clear neopixel

  /*

    All wifi code from before goes here

  */

  // Set up the web server to handle different routes
  server.on("/", handleRoot);       // Base case, runs constantly, checks if a button was pressed
  server.on("/on", On);             // Runs On() function
  server.on("/off", Off);           // Runs Off() function

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle incoming client requests
  server.handleClient();
}

```
Then a web server object is created on port 80 called ``server``:
```C
WebServer server(80);
```
If you want to create a button on the page, e.g. to turn on an LED, each button will lead to an extension of the URL. So if the IP was 192.168.1.1, what is called root would be:
1. ``192.168.1.1/``              <--- root
2. ``192.168.1.1/Button1_ON``    <--- Press button 1
3. ``192.168.1.1/Button1_OFF``   <--- Press button 1 again
Therefore, you need to set up the web server to handle all pages. With the button example:
```C
server.on("/", handleRoot);
server.on("/Button1_ON", handleButton1ON);
server.on("/Button1_OFF", handleButton1OFF);
```
Then you start the web server:
```C
server.begin();
```
In loop(), you continuously run a function called ``handleClient()``:
```C
// This function ensures that incoming requests from the browser are handled
server.handleClient();
```
As shown before, you need functions to handle when someone navigates to e.g. ``192.168.1.1/Button1_ON``. So you need handleButton1ON/OFF implemented. It can look like this:
```C
void handleButton1ON() {
    LEDState = "on";
    pixel.show();
    handleRoot();
}

void handleButton1OFF() {
    LEDState = "off";
    pixel.clear();
    handleRoot();
}
```
The function that generates the web page is ``handleRoot()``. It sends HTML and CSS to generate the page; the necessary HTML and CSS text is stored in the ``html`` variable.
```C
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\"
                 content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block;
           margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white;
           padding: 16px 40px; text-decoration: none; font-size: 30px;
           margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESP32 Web Server</h1>";

// Show the button based on LEDState
html += "<p>Button1 - State " + LEDState + "</p>";
  if (LEDState == "off") {
    html += "<p><a href=\"/Button1_ON\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/Button1_OFF\"><button
             class=\"button button2\">OFF</button></a></p>";
  }

}
```
Finally, in the same HTML block, the web page is sent to the phone or PC at the link.
```C
html += "</body></html>";
server.send(200, "text/html", html);
```
# Zigbee - Fundamental Functions
To get Zigbee working, the package must be included with:
```C
#include "Zigbee.h"
```
Then the role must be defined outside of setup() with:
```C
zigbee_role_t role = ZIGBEE_COORDINATOR; // ESP does not scan itself with this function
                                         // Only one coordinator per network
                                         // Starts and owns the network


zigbee_role_t role = ZIGBEE_ROUTER;      // ESP can communicate + scan
                                         // Forwards data between coordinator and other nodes.
                                         // Good balance between functionality and flexibility.

zigbee_role_t role = ZIGBEE_END_DEVICE;  // ESP acts as a sensor
                                         // Saves power, because it sleeps most of the time.


```
# Network Scanning
These code examples are for after the scan is complete; they are used in a function to print the scan results:
```C
zigbee_scan_result_t *scan_result = Zigbee.getScanResult();
```
The results from this scan can be retrieved with these commands:
```C
scan_result[i].short_pan_id         // Gets the 16-bit network ID, e.g. 0x4A8C
scan_result[i].logic_channel        // Gets the ZB channel (11-26), e.g. 15
scan_result[i].permit_joining       // Gets the joining permission status (YES/NO), e.g. YES
scan_result[i].router_capacity      // Gets whether there is room for more routers on the network, e.g. NO
scan_result[i].end_device_capacity  // Gets whether there is room for more end devices, e.g. YES
scan_result[i].extended_pan_id[j]   // Gets the extended 64-bit network ID (like a MAC address) e.g. a1:b2:c3...
```
OUTPUT EXAMPLE:
```C
0x4A8C | 15 | Yes           | No             | Yes                | 00:12:4b:00:1a:2b:3c:4d
```
After ``Zigbee.getScanResult();`` has been run, you run:
```C
Zigbee.scanDelete();
```
To free up the memory used.
# Setup
When inside ``void setup()`` you can start Zigbee on the ESP with the role that was set before setup and the fundamental functions:
```C
Zigbee.begin(role)
```
If it has started successfully, you can begin scanning:
```C
Zigbee.scanNetworks();
```