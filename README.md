# Introduktion
Only in danish.

Motivation var at jeg gerne ville styre min pære i mit kollegieværelse via min pc og mobil.  
  
Jeg ville også senere, hvis ikke allerede implementeret, at få en slags "alarm" som vækker mig nemmere om morgenen, ved at tænde for pæren ved den højeste temperatur (kelvin), og højeste lysstyrke (lumen).

ESP32 er en C6 model, så den understøtter Zigbee, en teknologi som tillader at man kan kommunikere med sensorer via en enkelt device.  


# LED - Neopixel
For at starte LED'en, skal man inkludere biblioteket og følgende kode:  
```C
#include <Adafruit_NeoPixel.h>      // Behøves faktisk ikke være dette bibliotek, men dette virker
#define NUMPIXELS 1                 // Antallet af pixels, kun en på ESP32'eren
#define PIN 8                       // LED'en er forbundet til pin 8
Adafruit_NeoPixel pixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
```  
  
  
Derefter kan man styre LED'en med disse funktioner:  
```C
pixel.begin();                                  // Obligatorisk

pixel.setPixelColor(0, pixel.Color(60, 0, 0));  // pixel.setPixelColor(0, ...) sætter farven for LED nummer 0
                                                // pixel.Color(60, 0, 0) sætter farven til 
                                                // rød 60/255, grøn 0/255 og blå 0/255
                                                
pixel.show();                                   // Bruges til at opdatere LED, altså efter man har kørt 
                                                // setPixelColor og clear()

pixel.clear();                                  // Slukker LED
``` 

# Wifi
Først, skal man importere biblioteket:  
```C
#include <WiFi.h>
```
Generelt at have denne funktion og rækkefølge som køres i setup(), burde få alt til at virke:  
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
#### Wifi statisk IP
Det kan være at man får tilfældig adresse hver gang pga. DHCP (Dynamic Host Configuration Protocol), så for at få den samme hver gang, skal man bruge dette udenfor ``setup()`` og ``loop()``
```C
if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Fejl i konfigurering")
}
```
Her er forklaring af parametre, de to sidste parametre er ikke nødvendige:
```C
// Statisk IP-adresse, må ikke være en brugt IP-adresse! Helst imellem 192.168.1.180-199
IPAddress local_IP(192, 168, 1, 184);   
  
// Gateway IP-adresse, dette er IP-adressen på den forbundne router
IPAddress gateway(192, 168, 8, 1); // På GLi-net router

// Definerer netværksmasken, fortæller hvilke IP'er der er lokale. 
// (255, 255, 255, 0) Betyder kun IP'er med de samme 3 første tal er lokale, denne er standard
// (255, 255, 0, 0) Betyder hele 192.168.. er lokalt.
IPAddress subnet(255, 255, 0, 0);

// Ikke vigtigt, kan bare være dette
IPAddress primaryDNS(8, 8, 8, 8);   // Google DNS
IPAddress secondaryDNS(8, 8, 4, 4); // Google DNS
```
#### Wifi status
I tabellen under forklares det som funktionen wifi.status() returnerer:
```C
WiFi.status()
```
|Værdi|String|Betydning|  
|-|-|-|  
|0|`WL_IDLE_STATUS`|Status givet når `WiFi.begin()` er kaldet|
|1|`WL_NO_SSID_AVAIL`|Ingen SSID er tilgængelige|
|2|`WL_SCAN_COMPLETED`|Scan networks er færdigt|
|3|`WL_CONNECTED`|Forbundet til netværk|
|4|`WL_CONNECT_FAILED`|Alle forsøg for forbindelse fejler|
|5|`WL_CONNECTION_LOST`|Forbindelse tabt|
|6|`WL_DISCONNECTED`|Afbrudt fra netværk|

#### Wifi modes
```C
WiFi.mode(WIFI_STA);    // Station mode, ESP32 forbinder til et access point
WiFi.mode(WIFI_AP);     // Access point mode, enheder kan forbinde til ESP32
WiFi.mode(WIFI_AP_STA); // Access point og station forbundet til et andet access point
```
#### Wifi localip
Funktionen her returnerer bare den lokale ip som man kan tilgå med en browser:  
```C
WiFi.localIP(); // returnerer f.eks. 192.168.8.227
```
# Webserver
Først inkluderer man biblioteket:  
```C
#include <WebServer.h>
```
Generelt at have denne funktion og rækkefølge som køres i setup(), burde få alt til at virke:  
```C
WebServer server(80); 


// Funktioner til at håndtere tryk på knapper eller andre aktioner på root
void Off() {
  outputState = "off";
  // Gør noget her, blink LED, whatever
  handleRoot();
}

void On() {
  outputState = "on";
  // Gør noget her, blink LED, whatever
  handleRoot();
}


void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESP32 Web Server</h1>";

  // Vis controls
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

  // Klargør neopixel

  // Clear neopixel

  /*

    Her er al wifi kode fra før

  */

  // Set up the web server to handle different routes
  server.on("/", handleRoot);       // Base-case, kører konstant, og tjekker om tryk er sket
  server.on("/on", On);             // Kører On() funktion
  server.on("/off", Off);           // Kører Off() funktion

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle incoming client requests
  server.handleClient();
}

```
Derefter laves et webserver objekt på port 80 kaldet ``server``:  
```C
WebServer server(80);
```
Hvis man vil lave en knap på siden, f.eks. til at tænde en LED, vil hver knap føre til en forlængelse af URL'en, altså, hvis IP var 192.168.1.1 så det der hedder root være:  
1. ``192.168.1.1/``            <--- root
2. ``192.168.1.1/Knap1_ON``    <--- Tryk på knap 1
3. ``192.168.1.1/Knap1_OFF``   <--- Tryk på knap 1 igen
Derfor, skal man sætte webserveren op til at kunne klare alle sider, med knap eksemplet:  
```C
server.on("/", handleRoot);
server.on("/Knap1_ON", handleKnap1ON);
server.on("/Knap1_OFF", handleKnap1OFF);
```
Herefter starter man web serveren:  
```C
server.begin();
```
I loop(), kører man konstant en funktion kaldet ``handleClient()``:  
```C
// Funktionen sørger for at indkommende requests fra browseren håndteres
server.handleClient();
```
Som blev vist før, skal man have funktioner for at håndtere hvis man f.eks. går ind på ``192.168.1.1/Knap1_ON``. Altså man skal have handleKnap1ON/OFF implementeret. Den kan f.eks. ligne:  
```C
void handleKnap1ON() {
    LEDState = "on";
    pixel.show();
    handleRoot();
}

void handleKnap1OFF() {
    LEDState = "off";
    pixel.clear();
    handleRoot();
}
```
Selve funktionen for at generere websiden er ``handleRoot()``, den sender HTML og CSS for at generere siden, HTML og CSS teksten nødvendig, gemmes i ``html`` variablen.  
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

// Vis knappen ift. LEDState
html += "<p>Knap1 - State " + LEDState + "</p>";
  if (LEDState == "off") {
    html += "<p><a href=\"/Knap1_ON\"><button class=\"button\">ON</button></a></p>";
  } else {
    html += "<p><a href=\"/Knap1_OFF\"><button 
             class=\"button button2\">OFF</button></a></p>";
  }

}
```
Til sidst, i samme HTML blok, sendes websiden til mobilen eller PC'en på linket.
```C
html += "</body></html>";
server.send(200, "text/html", html);
```
# Zigbee - Fundamentale funktioner
For at få ZB til at fungere, skal pakken inkluderes med:
```C
#include "Zigbee.h"
```
Derefter skal rollen defineres udenfor setup() med:
```C
zigbee_role_t role = ZIGBEE_COORDINATOR; // ESP skanner ikke sig selv med denne funktion
                                         // Kun en coordinator per netværk
                                         // Starter og ejer netværket


zigbee_role_t role = ZIGBEE_ROUTER;      // ESP kan kommunikere + kunne scanne
                                         // Videresender data mellem coordinator og andre noder.
                                         // God balance mellem funktionalitet og fleksibilitet.

zigbee_role_t role = ZIGBEE_END_DEVICE;  // ESP er som sensor
                                         // Sparer strøm, fordi den sover meget af tiden.


```
# Scanning af netværk
Disse kode eksempler er så efter skanningen er udført, de bruges i en funktion til at printe resultaterne fra skanningen:
```C
zigbee_scan_result_t *scan_result = Zigbee.getScanResult();
```
Resultaterne fra dette skan, kan fås med disse kommandoer:
```C
scan_result[i].short_pan_id         // Henter 16-bit netværks ID, f.eks. 0x4A8C
scan_result[i].logic_channel        // Henter ZB-kanal (11-26), f.eks. 15
scan_result[i].permit_joining       // Henter status på åbenhed (JA/NEJ), f.eks. JA
scan_result[i].router_capacity      // Henter om der er plads til flere routers på netværket, f.eks. NEJ
scan_result[i].end_device_capacity  // Henter om der er plads til flere end devices, f.eks. JA
scan_result[i].extended_pan_id[j]   // Henter den udvidede 64-bit netværks ID (som MAC-adresse) f.eks. a1:b2:c3...
```
OUTPUT-EKSEMPEL:  
```C
0x4A8C | 15 | Yes           | No             | Yes                | 00:12:4b:00:1a:2b:3c:4d
```
Når ``Zigbee.getScanResult();`` er kørt, kører man:  
```C
Zigbee.scanDelete();
```
For at spare på hukommelse brugt
# Setup
Når man er i ``void setup()`` kan man starte ZB på ESPen med den rolle man satte helt inden setup og de fundamentale funktioner:  
```C
Zigbee.begin(role)
``` 
Hvis den er begyndt succesfuldt, kan man starte skanningen:  
```C
Zigbee.scanNetworks();
```
