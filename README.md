# Motivation for dette
Min motivation stammer fra jeg ejer en Ikea TRÅDFRI pære som styres via en lille controller man fik med i pakken. Den er cool nok, men meget simpel. Jeg savner at jeg kan styre mit lys derhjemme via mine egne kommandoer der afhænger af tidspunktet, alarmer fra min mobil, notifikation fra min computer eller mobil.  
  
Alt er vel muligt, og det virker fedt at prøve grave ned i det. ChatGPT, Claude, Gemini osv. var til ikke så megen hjælp, så jeg vil prøve skrive det hele op for at få overblik over hvad det er jeg rent faktisk laver :).


# Fundamentale funktioner
For at få ZB til at fungere, skal biblioteket inkluderes med:
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