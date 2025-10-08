#include "esp_coexist.h" 
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include "Zigbee.h"
#include <math.h>

#define ZIGBEE_MODE_ZCZR
#define NUMPIXELS 1
#define PIN 8
#define EP_SWITCH 5

Adafruit_NeoPixel pixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

const char* ssid = "BeepBoopNet";
const char* password = "TKY7DEWKNZ";  

String outputState = "off";

WebServer server(80);
ZigbeeColorDimmerSwitch zbSwitch(EP_SWITCH);

bool blinked = false;
int ledState = LOW;        // husker LED'ens nuværende tilstand
int buttonState;           // den nuværende knaptilstand
int lastButtonState = LOW; // husker forrige knaptilstand

unsigned long lastDebounceTime = 0;  
unsigned long debounceDelay = 50;    // 50 ms debounce

int brightness;
int kelvin;

void toggleBOOTButton() {
  int reading = digitalRead(BOOT_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) {  
        ledState = !ledState;
        
        if (ledState == HIGH){
          zbSwitch.lightOn();
          pixel.setPixelColor(0, pixel.Color(0, 0, 10));
          pixel.show();
        } else {
          zbSwitch.lightOff();
          pixel.clear();
          pixel.show();
        }
      }
    }
  }
  
  lastButtonState = reading;
}

void pulseLED() {
  int i;
  for (i = 0; i < 256; i++) {
    pixel.setPixelColor(0, pixel.Color(0, 0, i)); 
    pixel.show();
  }

  for (i = 256; i > 0; i--) {
    pixel.setPixelColor(0, pixel.Color(0, 0, i));
    pixel.show();
  }

  delay(700);
}

void blinkThree() { 
  for(uint8_t i=0; i<3; i++) { 
    pixel.setPixelColor(0, pixel.Color(0, 90, 0));
    pixel.show();
    delay(150); 
    pixel.clear();
    pixel.show(); 
    delay(150);
    } 
}

void handleSetBulb() {
  // Læs parametrene fra URL'en
  if (server.hasArg("brightness")) {
    int brightness = server.arg("brightness").toInt();
    zbSwitch.setLightLevel(map(brightness, 0, 1055, 0, 255));  
  }
  
  if (server.hasArg("temperature")) {
    byte r, g, b;
    int kelvin = server.arg("temperature").toInt();
    //zbSwitch.setLightColor(r, g, b);
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
  html += "  var brightness = document.getElementById('brightness').value;";
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

void setBulbTemperature(int kelvin) {
  

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

  toggleBOOTButton();

  if(!zbSwitch.bound()) {
    pulseLED();
  }

  if(!blinked && zbSwitch.bound()) { 
    Serial.println("Pære forbundet");   
    blinkThree(); 
    blinked = true; 
    }

  server.handleClient();
}

