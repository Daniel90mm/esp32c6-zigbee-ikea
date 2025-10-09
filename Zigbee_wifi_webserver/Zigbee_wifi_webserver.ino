#include "esp_coexist.h" // Sørger for at wifi og zigbee kan være tændt samtidigt
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include "Zigbee.h"
#include <math.h>

#define ZIGBEE_MODE_ZCZR
#define NUMPIXELS 1
#define PIN 8
#define EP_SWITCH 5 // ikea pære endpoint

Adafruit_NeoPixel pixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

const char* ssid = "BeepBoopNet";
const char* password = "TKY7DEWKNZ";  

String outputState = "off";

WebServer server(80);
ZigbeeColorDimmerSwitch zbSwitch(EP_SWITCH);
ZigbeeColorDimmableLight zbColorLight(EP_SWITCH);

bool blinked = false;
int brightness;
int kelvin;

void pulseLED(String color) {
  int i;
  if (color == "red") {
  for (i = 0; i < 256; i++) {
    pixel.setPixelColor(0, pixel.Color(i, 0, 0)); 
    pixel.show();
  }

  for (i = 256; i > 0; i--) {
    pixel.setPixelColor(0, pixel.Color(i, 0, 0));
    pixel.show();
    }
  pixel.clear();
  pixel.show();
  }

  if (color == "green") {
  for (i = 0; i < 256; i++) {
    pixel.setPixelColor(0, pixel.Color(0, i, 0)); 
    pixel.show();
  }

  for (i = 256; i > 0; i--) {
    pixel.setPixelColor(0, pixel.Color(0, i, 0));
    pixel.show();
    }
  pixel.clear();
  pixel.show();
  }

  if (color == "blue") {
  for (i = 0; i < 256; i++) {
    pixel.setPixelColor(0, pixel.Color(0, 0, i)); 
    pixel.show();
  }

  for (i = 256; i > 0; i--) {
    pixel.setPixelColor(0, pixel.Color(0, 0, i));
    pixel.show();
    }
  pixel.clear();
  pixel.show();
  }

}

void handleSetBulb() {
  // Læs parametrene fra URL'en
  if (server.hasArg("brightness")) {
    int brightness = server.arg("brightness").toInt();
    int mappedBrightness = map(brightness, 0, 1055, 0, 255)

    if (mappedBrightness < 7){
      zbSwitch.lightOff();
    }
    else {
      zbSwitch.lightOn();
      zbSwitch.setLightLevel(mappedBrightness);  
    }
  }
  
  if (server.hasArg("temperature")) {
    byte r, g, b;
    int kelvin = server.arg("temperature").toInt();
  }
  
  server.send(200, "text/plain", "OK");
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(WiFi.localIP());

  WiFi.setSleep(WIFI_PS_MIN_MODEM);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html>";
 
  html += "<head>";
  html += "<script>";
  html += "function updateBulb() {";
  html += "  var sliderVal = document.getElementById('brightness').value;";
  html += "  var brightness = Math.pow(sliderVal / 1055, 2) * 1055;"; // ekstra følsomhed for slider ved lave værdier
  html += "  brightness = Math.round(brightness);";
  html += "  var temp = document.getElementById('temperature').value;";
  html += "  var url = '/set?brightness=' + brightness + '&temperature=' + temp;";  
  html += "  fetch(url);";
  html += "  document.getElementById('brightnessValue').innerText = brightness;";  
  html += "  document.getElementById('tempValue').innerText = temp;";
  html += "}";
  html += "</script>";
 
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
 
  html += "<style>";
  html += "body { font-family: Helvetica; text-align: center; }";
  html += ".slider-container { margin: 20px; }";
  html += "input[type='range'] { width: 80%; height: 30px; }";
  html += "</style>";
  html += "</head>";
  
  html += "<body>";
  html += "<h1>Light Bulb Control</h1>";
 
  html += "<div class=\"slider-container\">";
  html += "  <label>Brightness: <span id=\"brightnessValue\">500</span> lumens</label><br>";
  html += "  <input type=\"range\" id=\"brightness\" min=\"0\" max=\"1055\" value=\"500\" onchange=\"updateBulb()\">";  
  html += "</div>";
 
  html += "<div class=\"slider-container\">";
  html += "  <label>Temperature: <span id=\"tempValue\">3500</span> K</label><br>";
  html += "  <input type=\"range\" id=\"temperature\" min=\"2700\" max=\"10000\" value=\"3500\" onchange=\"updateBulb()\">";  
  html += "</div>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void setBulbTemperature() {
  espRgbColor_t zbColor = zbColorLight.getLightColor();
  Serial.println(zbColor);

}

void setup() {

  Serial.begin(115200);
  pixel.begin();
  initWiFi();
  esp_coex_wifi_i154_enable(); // skal være lige efter initWiFi
  
  Zigbee.addEndpoint(&zbSwitch);
  if(!Zigbee.begin(ZIGBEE_COORDINATOR)){
    Serial.println("Zigbee.begin virker ikke");
    pixel.setPixelColor(0, pixel.Color(90, 0, 0)); 
    pixel.show();
  }
  
  pinMode(BOOT_PIN, INPUT_PULLUP);

  server.on("/", handleRoot);
  server.on("/set", handleSetBulb);    
  server.begin();
  Serial.println("HTTP server started");

}

void loop() {

  if(!zbSwitch.bound()) { // Hvis zigbee ikke forbundet
    pulseLED("red");
  }

  if(!blinked && zbSwitch.bound()) { 
    Serial.println("Pære forbundet");   
    pulseLED("green"); 
    blinked = true; 
    }

  server.handleClient();
}

