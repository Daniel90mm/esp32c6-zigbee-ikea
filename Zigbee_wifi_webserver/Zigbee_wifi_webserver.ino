#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include "Zigbee.h"
#include "esp_coexist.h"
#include <math.h>
#include <time.h>

#ifndef ZIGBEE_MODE_ZCZR
#error "Select Tools > Zigbee mode > Zigbee ZCZR (coordinator/router)"
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy secrets.example.h to secrets.h and enter your Wi-Fi credentials"
#endif

constexpr uint8_t SWITCH_ENDPOINT = 5;
constexpr uint8_t PIXEL_PIN = 8;
constexpr uint8_t OFF_BUTTON_PIN = 9;
constexpr uint8_t PAIRING_SECONDS = 180;
constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
constexpr uint32_t PAIRING_REOPEN_MS = 170000;
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
constexpr int MIN_KELVIN = 2200;
constexpr int MAX_KELVIN = 4000;
constexpr char TZ_COPENHAGEN[] = "CET-1CEST,M3.5.0,M10.5.0/3";

WebServer server(80);
Adafruit_NeoPixel pixel(1, PIXEL_PIN, NEO_GRB + NEO_KHZ800);
ZigbeeColorDimmerSwitch zbSwitch(SWITCH_ENDPOINT);
Preferences preferences;

enum class StatusLedMode {
  WIFI_CONNECTING,
  ZIGBEE_STARTING,
  PAIRING,
  PAIRED,
  ERROR
};

enum class WakePhase {
  IDLE,
  PREWAKE,
  BRIGHT,
  CALM
};

uint8_t currentBrightness = 50;  // Percent shown in the UI.
int currentKelvin = 3500;
bool currentPower = false;
bool wasBound = false;
int lastSentKelvin = 3500;
uint32_t lastPairingOpen = 0;
uint32_t lastWiFiRetry = 0;
StatusLedMode statusLedMode = StatusLedMode::WIFI_CONNECTING;

int alarmHour = 6;
int alarmMinute = 30;
bool alarmEnabled = true;
int lastAlarmDay = -1;
WakePhase wakePhase = WakePhase::IDLE;
bool wakeRoutineRequiresPhone = false;
uint32_t wakeRoutineStarted = 0;
uint32_t lastPhoneHeartbeat = 0;
time_t lastPhoneHeartbeatEpoch = 0;
time_t phoneAlarmEpoch = 0;
time_t lastTriggerEpoch = 0;
String lastTriggerSource = "none";
bool phoneAlarmPrewakeFired = false;
bool phoneAlarmWakeFired = false;
bool mdnsStarted = false;
bool buttonLastReading = HIGH;
bool buttonStableState = HIGH;
uint32_t buttonLastChanged = 0;

bool preWakeEnabled = false;
uint8_t preWakeMinutes = 10;
uint8_t preWakeBrightness = 10;
int preWakeKelvin = 2200;
uint8_t wakeBrightness = 100;
int wakeKelvin = 4000;
uint8_t calmAfterMinutes = 10;
uint8_t calmBrightness = 35;
int calmKelvin = 2700;
uint8_t offAfterMinutes = 20;
uint8_t phoneAbsenceMinutes = 5;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Zigbee light</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui,sans-serif; }
    body { max-width: 34rem; margin: 0 auto; padding: 1.25rem; background:#111827; color:#f9fafb; }
    main { padding:1.4rem; border-radius:1rem; background:#1f2937; box-shadow:0 12px 40px #0006; }
    h1 { margin-top:0; }
    .status { padding:.7rem; border-radius:.6rem; background:#111827; }
    .row { display:flex; gap:.7rem; margin:1rem 0; }
    button { flex:1; border:0; border-radius:.65rem; padding:.85rem; font-size:1rem; font-weight:700; cursor:pointer; }
    .on { background:#fbbf24; color:#111827; } .off { background:#374151; color:white; }
    .pair { background:#2563eb; color:white; }
    .alarm { margin-top:1.5rem; padding-top:1.25rem; border-top:1px solid #374151; }
    select, input[type=number] { padding:.65rem; border:1px solid #4b5563; border-radius:.5rem; background:#111827; color:white; font-size:1rem; }
    .time-picker { display:flex; align-items:center; gap:.4rem; margin-top:.5rem; font-size:1.4rem; }
    .grid { display:grid; grid-template-columns:1fr 1fr; gap:.8rem; }
    .grid label { margin-top:.4rem; }
    .grid input { box-sizing:border-box; width:100%; margin-top:.3rem; }
    details { margin-top:1rem; }
    summary { cursor:pointer; font-weight:700; }
    input[type=checkbox] { width:1.15rem; height:1.15rem; vertical-align:middle; }
    label { display:block; margin-top:1.2rem; }
    input[type=range] { width:100%; height:2rem; }
    small { color:#9ca3af; }
  </style>
</head>
<body><main>
  <h1>IKEA Zigbee light</h1>
  <p id="status" class="status">Loading status…</p>
  <label>Brightness: <strong id="brightnessValue">50%</strong>
    <input id="brightness" type="range" min="0" max="100" value="50">
  </label>
  <label>White tone: <strong id="tempValue">3500 K</strong>
    <input id="temperature" type="range" min="2200" max="4000" step="50" value="3500">
  </label>
  <section class="alarm">
    <h2>Wake light</h2>
    <p id="phoneAlarmStatus" class="status">Waiting for phone alarm sync…</p>
    <label><input id="alarmEnabled" type="checkbox"> Enable backup daily ESP32 alarm</label>
    <label>Time — Danish 24-hour clock
      <span class="time-picker">
        <select id="alarmHour" aria-label="Hour"></select>
        <span>:</span>
        <select id="alarmMinute" aria-label="Minute"></select>
      </span>
    </label>
    <div class="row">
      <button class="pair" onclick="saveAlarm()">Save alarm</button>
      <button class="on" onclick="command('/wake')">Test now</button>
    </div>
    <small id="alarmStatus">Loading alarm…</small>
    <details>
      <summary>Routine settings</summary>
      <label><input id="preWakeEnabled" type="checkbox"> Gentle pre-wake before the alarm</label>
      <div class="grid">
        <label>Pre-wake lead (min)<input id="preWakeMinutes" type="number" min="1" max="60"></label>
        <label>Pre-wake brightness (%)<input id="preWakeBrightness" type="number" min="1" max="100"></label>
        <label>Pre-wake tone (K)<input id="preWakeKelvin" type="number" min="2200" max="4000" step="50"></label>
        <label>Alarm brightness (%)<input id="wakeBrightness" type="number" min="1" max="100"></label>
        <label>Alarm tone (K)<input id="wakeKelvin" type="number" min="2200" max="4000" step="50"></label>
        <label>Calm after (min)<input id="calmAfterMinutes" type="number" min="1" max="120"></label>
        <label>Calm brightness (%)<input id="calmBrightness" type="number" min="1" max="100"></label>
        <label>Calm tone (K)<input id="calmKelvin" type="number" min="2200" max="4000" step="50"></label>
        <label>Automatic off (min)<input id="offAfterMinutes" type="number" min="2" max="180"></label>
        <label>Phone absence (min)<input id="phoneAbsenceMinutes" type="number" min="1" max="30"></label>
      </div>
      <div class="row"><button class="pair" onclick="saveRoutine()">Save routine</button></div>
    </details>
  </section>
  <small>Keep mains power on. A short press of the ESP32 BOOT button turns the bulb off safely.</small>
</main>
<script>
  const brightness = document.querySelector('#brightness');
  const temperature = document.querySelector('#temperature');
  const brightnessValue = document.querySelector('#brightnessValue');
  const tempValue = document.querySelector('#tempValue');
  const statusElement = document.querySelector('#status');
  const alarmEnabled = document.querySelector('#alarmEnabled');
  const alarmHour = document.querySelector('#alarmHour');
  const alarmMinute = document.querySelector('#alarmMinute');
  const alarmStatus = document.querySelector('#alarmStatus');
  const phoneAlarmStatus = document.querySelector('#phoneAlarmStatus');
  const routineIds = ['preWakeMinutes','preWakeBrightness','preWakeKelvin',
    'wakeBrightness','wakeKelvin','calmAfterMinutes','calmBrightness',
    'calmKelvin','offAfterMinutes','phoneAbsenceMinutes'];
  let routineLoaded = false;
  let timer;
  for (let hour = 0; hour < 24; hour++) {
    const value = String(hour).padStart(2, '0');
    alarmHour.add(new Option(value, value));
  }
  for (let minute = 0; minute < 60; minute++) {
    const value = String(minute).padStart(2, '0');
    alarmMinute.add(new Option(value, value));
  }
  async function command(path) {
    const response = await fetch(path, {method:'POST'});
    if (!response.ok) alert(await response.text());
    await refresh();
  }
  function changed() {
    brightnessValue.textContent = brightness.value + '%';
    tempValue.textContent = temperature.value + ' K';
    clearTimeout(timer);
    timer = setTimeout(() => command('/set?brightness=' + brightness.value +
      '&temperature=' + temperature.value), 180);
  }
  async function saveAlarm() {
    await command('/alarm?enabled=' + (alarmEnabled.checked ? '1' : '0') +
      '&hour=' + alarmHour.value + '&minute=' + alarmMinute.value);
  }
  async function saveRoutine() {
    const query = new URLSearchParams({
      preWakeEnabled: document.querySelector('#preWakeEnabled').checked ? '1' : '0'
    });
    routineIds.forEach(id => query.set(id, document.querySelector('#' + id).value));
    const response = await fetch('/routine?' + query, {method:'POST'});
    if (!response.ok) alert(await response.text());
    routineLoaded = false;
    await refresh();
  }
  function formatEpoch(epoch) {
    if (!epoch) return 'No native phone alarm is currently enabled';
    return 'Next phone alarm: ' + new Intl.DateTimeFormat('da-DK', {
      weekday:'short', day:'2-digit', month:'2-digit', hour:'2-digit',
      minute:'2-digit', hour12:false
    }).format(new Date(epoch * 1000));
  }
  async function refresh() {
    const state = await fetch('/status').then(r => r.json());
    statusElement.textContent = state.bound
      ? 'Bulb paired · light ' + (state.power ? 'on' : 'off')
      : 'Waiting for a bulb to pair';
    brightness.value = state.brightness;
    temperature.value = state.temperature;
    brightnessValue.textContent = state.brightness + '%';
    tempValue.textContent = state.temperature + ' K';
    alarmEnabled.checked = state.alarm.enabled;
    const [savedHour, savedMinute] = state.alarm.time.split(':');
    if (document.activeElement !== alarmHour) alarmHour.value = savedHour;
    if (document.activeElement !== alarmMinute) alarmMinute.value = savedMinute;
    phoneAlarmStatus.textContent = formatEpoch(state.phoneAlarm.epoch) +
      ' · phone ' + (state.phone.present ? 'seen on Wi-Fi' : 'not recently seen');
    if (state.lastTrigger.epoch) {
      phoneAlarmStatus.textContent += ' · last trigger: ' + state.lastTrigger.source +
        ' at ' + new Intl.DateTimeFormat('da-DK', {hour:'2-digit',minute:'2-digit',
          second:'2-digit',hour12:false}).format(new Date(state.lastTrigger.epoch * 1000));
    }
    if (!routineLoaded) {
      document.querySelector('#preWakeEnabled').checked = state.routine.preWakeEnabled;
      routineIds.forEach(id => document.querySelector('#' + id).value = state.routine[id]);
      routineLoaded = true;
    }
    alarmStatus.textContent = state.alarm.enabled
      ? 'Daily at ' + state.alarm.time + (state.timeSynced ? '' : ' — waiting for internet time')
      : 'Daily ESP32 alarm disabled; phone trigger remains available';
    if (state.wakeRoutine.active) {
      alarmStatus.textContent = 'Wake routine: ' + state.wakeRoutine.phase;
    }
  }
  brightness.addEventListener('input', changed);
  temperature.addEventListener('input', changed);
  refresh();
  setInterval(refresh, 5000);
</script></body></html>
)HTML";

void setPixel(uint8_t red, uint8_t green, uint8_t blue) {
  pixel.setPixelColor(0, pixel.Color(red, green, blue));
  pixel.show();
}

void setStatusLedMode(StatusLedMode mode) {
  statusLedMode = mode;
}

void updateStatusPixel() {
  const uint32_t now = millis();

  switch (statusLedMode) {
    case StatusLedMode::WIFI_CONNECTING:
      // Blue blink: joining the configured 2.4 GHz Wi-Fi.
      if ((now / 300) % 2 == 0) setPixel(0, 0, 35);
      else setPixel(0, 0, 0);
      break;

    case StatusLedMode::ZIGBEE_STARTING:
      // Purple blink: Wi-Fi is ready and the Zigbee coordinator is starting.
      if ((now / 300) % 2 == 0) setPixel(24, 0, 35);
      else setPixel(0, 0, 0);
      break;

    case StatusLedMode::PAIRING: {
      // Amber breathing: pairing is open and waiting for the reset bulb.
      const uint16_t phase = now % 1600;
      const uint8_t level = phase < 800 ? map(phase, 0, 799, 3, 38)
                                        : map(phase, 800, 1599, 38, 3);
      setPixel(level, level / 4, 0);
      break;
    }

    case StatusLedMode::PAIRED:
      // Solid green: the bulb is paired and commands can be sent.
      setPixel(0, 28, 0);
      break;

    case StatusLedMode::ERROR:
      // Fast red blink: firmware startup failed.
      if ((now / 120) % 2 == 0) setPixel(45, 0, 0);
      else setPixel(0, 0, 0);
      break;
  }
}

void sendLightOn() {
  // The original sketch's binding-based commands are known to work with this
  // IKEA bulb. Do not replace these with direct IEEE-address commands.
  zbSwitch.lightOn();
}

void sendLightOff() {
  zbSwitch.lightOff();
}

void sendLightToggle() {
  zbSwitch.lightToggle();
}

void sendLightLevel(uint8_t level) {
  zbSwitch.setLightLevel(level);
}

void sendLightTemperature(int kelvin) {
  esp_zb_zcl_color_move_to_color_temperature_cmd_t command = {};
  command.zcl_basic_cmd.src_endpoint = SWITCH_ENDPOINT;
  command.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  command.color_temperature = 1000000UL / kelvin;  // Zigbee uses mireds.
  command.transition_time = 5;                     // 0.5-second transition.

  if (esp_zb_lock_acquire(portMAX_DELAY)) {
    esp_zb_zcl_color_move_to_color_temperature_cmd_req(&command);
    esp_zb_lock_release();
  }
}

bool requireBound() {
  if (zbSwitch.bound()) {
    return true;
  }
  server.send(409, "text/plain",
              "No bulb is paired yet. Reset the bulb; pairing opens automatically.");
  return false;
}

void applyLight() {
  if (!zbSwitch.bound()) {
    return;
  }

  if (currentBrightness == 0) {
    currentPower = false;
    sendLightOff();
    return;
  }

  currentPower = true;
  sendLightOn();
  delay(30);
  // Match the old, proven sketch: raw Zigbee level 0-255 over the binding.
  sendLightLevel(map(currentBrightness, 1, 100, 1, 255));

  if (currentKelvin != lastSentKelvin) {
    delay(30);
    sendLightTemperature(currentKelvin);
    lastSentKelvin = currentKelvin;
  }
}

void handleSetBulb() {
  if (!requireBound()) {
    return;
  }
  if (server.hasArg("brightness")) {
    currentBrightness = constrain(server.arg("brightness").toInt(), 0, 100);
  }
  if (server.hasArg("temperature")) {
    currentKelvin = constrain(server.arg("temperature").toInt(), MIN_KELVIN, MAX_KELVIN);
  }
  wakePhase = WakePhase::IDLE;
  wakeRoutineRequiresPhone = false;
  applyLight();
  server.send(200, "application/json", "{\"ok\":true}");
}

const char *wakePhaseName() {
  switch (wakePhase) {
    case WakePhase::PREWAKE: return "pre-wake";
    case WakePhase::BRIGHT: return "bright";
    case WakePhase::CALM: return "calm";
    default: return "idle";
  }
}

bool phoneRecentlySeen() {
  return lastPhoneHeartbeat != 0 && millis() - lastPhoneHeartbeat < 150000UL;
}

void handleStatus() {
  String json = "{\"bound\":";
  json += zbSwitch.bound() ? "true" : "false";
  json += ",\"power\":";
  json += currentPower ? "true" : "false";
  json += ",\"brightness\":" + String(currentBrightness);
  json += ",\"temperature\":" + String(currentKelvin);
  json += ",\"timeSynced\":";
  json += time(nullptr) > 1700000000 ? "true" : "false";
  char alarmTime[6];
  snprintf(alarmTime, sizeof(alarmTime), "%02d:%02d", alarmHour, alarmMinute);
  json += ",\"alarm\":{\"enabled\":";
  json += alarmEnabled ? "true" : "false";
  json += ",\"time\":\"" + String(alarmTime) + "\"}";
  json += ",\"phoneAlarm\":{\"epoch\":" + String((uint64_t)phoneAlarmEpoch) + "}";
  json += ",\"phone\":{\"present\":";
  json += phoneRecentlySeen() ? "true" : "false";
  json += ",\"lastHeartbeat\":" + String((uint64_t)lastPhoneHeartbeatEpoch) + "}";
  json += ",\"lastTrigger\":{\"epoch\":" + String((uint64_t)lastTriggerEpoch);
  json += ",\"source\":\"" + lastTriggerSource + "\"}";
  json += ",\"routine\":{\"preWakeEnabled\":";
  json += preWakeEnabled ? "true" : "false";
  json += ",\"preWakeMinutes\":" + String(preWakeMinutes);
  json += ",\"preWakeBrightness\":" + String(preWakeBrightness);
  json += ",\"preWakeKelvin\":" + String(preWakeKelvin);
  json += ",\"wakeBrightness\":" + String(wakeBrightness);
  json += ",\"wakeKelvin\":" + String(wakeKelvin);
  json += ",\"calmAfterMinutes\":" + String(calmAfterMinutes);
  json += ",\"calmBrightness\":" + String(calmBrightness);
  json += ",\"calmKelvin\":" + String(calmKelvin);
  json += ",\"offAfterMinutes\":" + String(offAfterMinutes);
  json += ",\"phoneAbsenceMinutes\":" + String(phoneAbsenceMinutes) + "}";
  json += ",\"wakeRoutine\":{\"active\":";
  json += wakePhase != WakePhase::IDLE ? "true" : "false";
  json += ",\"phase\":\"" + String(wakePhaseName()) + "\",\"phoneRequired\":";
  json += wakeRoutineRequiresPhone ? "true" : "false";
  json += "}}";
  server.send(200, "application/json", json);
}

void recordTrigger(const char *source) {
  lastTriggerEpoch = time(nullptr);
  lastTriggerSource = source;
  preferences.putULong64("lastTrigAt", (uint64_t)lastTriggerEpoch);
  preferences.putString("lastTrig", lastTriggerSource);
}

void notePhoneHeartbeat() {
  lastPhoneHeartbeat = millis();
  lastPhoneHeartbeatEpoch = time(nullptr);
}

void triggerPreWake() {
  currentBrightness = preWakeBrightness;
  currentKelvin = preWakeKelvin;
  wakePhase = WakePhase::PREWAKE;
  wakeRoutineRequiresPhone = true;
  applyLight();
  recordTrigger("synced pre-wake");
  Serial.printf("Pre-wake started: %u%% at %d K.\n", currentBrightness, currentKelvin);
}

void triggerWakeLight(bool requirePhonePresence, const char *source) {
  currentBrightness = wakeBrightness;
  currentKelvin = wakeKelvin;
  wakePhase = WakePhase::BRIGHT;
  wakeRoutineRequiresPhone = requirePhonePresence;
  wakeRoutineStarted = millis();
  applyLight();
  recordTrigger(source);
  Serial.printf("Wake routine started: %u%% at %d K (%s).\n",
                currentBrightness, currentKelvin, source);
}

void finishWakeRoutine(const char *reason) {
  wakePhase = WakePhase::IDLE;
  wakeRoutineRequiresPhone = false;
  currentPower = false;
  sendLightOff();
  Serial.printf("Wake routine ended: %s.\n", reason);
}

void checkWakeRoutine() {
  if ((wakePhase != WakePhase::BRIGHT && wakePhase != WakePhase::CALM) ||
      !zbSwitch.bound()) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t elapsed = now - wakeRoutineStarted;
  const uint32_t calmAfterMs = (uint32_t)calmAfterMinutes * 60000UL;
  const uint32_t autoOffMs = (uint32_t)offAfterMinutes * 60000UL;
  const uint32_t absenceMs = (uint32_t)phoneAbsenceMinutes * 60000UL;

  if (elapsed >= autoOffMs) {
    finishWakeRoutine("automatic timeout");
    return;
  }

  if (wakeRoutineRequiresPhone && elapsed >= calmAfterMs &&
      (lastPhoneHeartbeat == 0 || now - lastPhoneHeartbeat >= absenceMs)) {
    finishWakeRoutine("phone absent from Wi-Fi");
    return;
  }

  if (wakePhase == WakePhase::BRIGHT && elapsed >= calmAfterMs) {
    wakePhase = WakePhase::CALM;
    currentBrightness = calmBrightness;
    currentKelvin = calmKelvin;
    applyLight();
    Serial.printf("Wake routine changed to %u%% at %d K.\n",
                  currentBrightness, currentKelvin);
  }
}

void handleRoutineUpdate() {
  const char *required[] = {
    "preWakeEnabled", "preWakeMinutes", "preWakeBrightness", "preWakeKelvin",
    "wakeBrightness", "wakeKelvin", "calmAfterMinutes", "calmBrightness",
    "calmKelvin", "offAfterMinutes", "phoneAbsenceMinutes"
  };
  for (const char *name : required) {
    if (!server.hasArg(name)) {
      server.send(400, "text/plain", "Missing routine setting: " + String(name));
      return;
    }
  }

  const int newPreMinutes = server.arg("preWakeMinutes").toInt();
  const int newPreBrightness = server.arg("preWakeBrightness").toInt();
  const int newPreKelvin = server.arg("preWakeKelvin").toInt();
  const int newWakeBrightness = server.arg("wakeBrightness").toInt();
  const int newWakeKelvin = server.arg("wakeKelvin").toInt();
  const int newCalmAfter = server.arg("calmAfterMinutes").toInt();
  const int newCalmBrightness = server.arg("calmBrightness").toInt();
  const int newCalmKelvin = server.arg("calmKelvin").toInt();
  const int newOffAfter = server.arg("offAfterMinutes").toInt();
  const int newAbsence = server.arg("phoneAbsenceMinutes").toInt();

  if (newPreMinutes < 1 || newPreMinutes > 60 ||
      newPreBrightness < 1 || newPreBrightness > 100 ||
      newPreKelvin < MIN_KELVIN || newPreKelvin > MAX_KELVIN ||
      newWakeBrightness < 1 || newWakeBrightness > 100 ||
      newWakeKelvin < MIN_KELVIN || newWakeKelvin > MAX_KELVIN ||
      newCalmAfter < 1 || newCalmAfter > 120 ||
      newCalmBrightness < 1 || newCalmBrightness > 100 ||
      newCalmKelvin < MIN_KELVIN || newCalmKelvin > MAX_KELVIN ||
      newOffAfter <= newCalmAfter || newOffAfter > 180 ||
      newAbsence < 1 || newAbsence > 30) {
    server.send(400, "text/plain",
                "Invalid routine settings. Automatic off must be after the calm transition.");
    return;
  }

  preWakeEnabled = server.arg("preWakeEnabled") == "1";
  preWakeMinutes = newPreMinutes;
  preWakeBrightness = newPreBrightness;
  preWakeKelvin = newPreKelvin;
  wakeBrightness = newWakeBrightness;
  wakeKelvin = newWakeKelvin;
  calmAfterMinutes = newCalmAfter;
  calmBrightness = newCalmBrightness;
  calmKelvin = newCalmKelvin;
  offAfterMinutes = newOffAfter;
  phoneAbsenceMinutes = newAbsence;

  preferences.putBool("preOn", preWakeEnabled);
  preferences.putUChar("preMin", preWakeMinutes);
  preferences.putUChar("preBri", preWakeBrightness);
  preferences.putUShort("preK", preWakeKelvin);
  preferences.putUChar("wakeBri", wakeBrightness);
  preferences.putUShort("wakeK", wakeKelvin);
  preferences.putUChar("calmMin", calmAfterMinutes);
  preferences.putUChar("calmBri", calmBrightness);
  preferences.putUShort("calmK", calmKelvin);
  preferences.putUChar("offMin", offAfterMinutes);
  preferences.putUChar("absMin", phoneAbsenceMinutes);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handlePhoneAlarmUpdate() {
  if (!server.hasArg("epoch")) {
    server.send(400, "text/plain", "Required parameter: epoch");
    return;
  }

  const uint64_t newEpoch = strtoull(server.arg("epoch").c_str(), nullptr, 10);
  if (newEpoch != (uint64_t)phoneAlarmEpoch) {
    phoneAlarmEpoch = (time_t)newEpoch;
    phoneAlarmPrewakeFired = false;
    phoneAlarmWakeFired = false;
    preferences.putULong64("phoneAlarm", newEpoch);
  }

  if (phoneAlarmEpoch == 0 && wakePhase == WakePhase::PREWAKE) {
    finishWakeRoutine("phone alarm cancelled during pre-wake");
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleAlarmUpdate() {
  if (!server.hasArg("enabled") || !server.hasArg("hour") || !server.hasArg("minute")) {
    server.send(400, "text/plain", "Required parameters: enabled, hour, minute");
    return;
  }

  const int hour = server.arg("hour").toInt();
  const int minute = server.arg("minute").toInt();
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    server.send(400, "text/plain", "Invalid alarm time");
    return;
  }

  alarmEnabled = server.arg("enabled") == "1" || server.arg("enabled") == "true";
  alarmHour = hour;
  alarmMinute = minute;
  lastAlarmDay = -1;
  preferences.putBool("alarmOn", alarmEnabled);
  preferences.putUChar("alarmHour", alarmHour);
  preferences.putUChar("alarmMinute", alarmMinute);
  server.send(200, "application/json", "{\"ok\":true}");
}

void startWiFi() {
  setStatusLedMode(StatusLedMode::WIFI_CONNECTING);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to Wi-Fi \"%s\"", WIFI_SSID);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_TIMEOUT_MS) {
    updateStatusPixel();
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
    Serial.printf("Web control: http://%s/\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin("wake-light")) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
      Serial.println("Stable local name: http://wake-light.local/");
    }
    return;
  }

  Serial.println("Wi-Fi not connected yet; Zigbee will still start and Wi-Fi will retry.");
  lastWiFiRetry = millis();
}

void configureWebServer() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE); });
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/set", HTTP_POST, handleSetBulb);
  server.on("/alarm", HTTP_POST, handleAlarmUpdate);
  server.on("/routine", HTTP_POST, handleRoutineUpdate);
  server.on("/phone-alarm", HTTP_POST, handlePhoneAlarmUpdate);
  server.on("/wake", HTTP_POST, []() {
    if (!requireBound()) return;
    notePhoneHeartbeat();
    const time_t now = time(nullptr);
    if (phoneAlarmEpoch > 0 && llabs((long long)now - (long long)phoneAlarmEpoch) <= 120) {
      phoneAlarmWakeFired = true;
    }
    triggerWakeLight(true, "phone alarm event");
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/presence", HTTP_POST, []() {
    notePhoneHeartbeat();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/on", HTTP_POST, []() {
    if (!requireBound()) return;
    wakePhase = WakePhase::IDLE;
    wakeRoutineRequiresPhone = false;
    if (currentBrightness == 0) currentBrightness = 50;
    currentPower = true;
    applyLight();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/off", HTTP_POST, []() {
    if (!requireBound()) return;
    wakePhase = WakePhase::IDLE;
    wakeRoutineRequiresPhone = false;
    currentPower = false;
    sendLightOff();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/pair", HTTP_POST, []() {
    Zigbee.openNetwork(PAIRING_SECONDS);
    lastPairingOpen = millis();
    Serial.println("Zigbee pairing opened for 180 seconds.");
    server.send(200, "application/json", "{\"ok\":true,\"pairingSeconds\":180}");
  });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void checkAlarm() {
  if (!alarmEnabled || !zbSwitch.bound()) {
    return;
  }

  struct tm localTime;
  if (!getLocalTime(&localTime, 0)) {
    return;
  }

  if (localTime.tm_hour == alarmHour && localTime.tm_min == alarmMinute &&
      localTime.tm_yday != lastAlarmDay) {
    triggerWakeLight(false, "ESP daily alarm");
    lastAlarmDay = localTime.tm_yday;
    Serial.println("Daily alarm fired.");
  }
}

void checkPhoneAlarm() {
  if (phoneAlarmEpoch <= 0 || !zbSwitch.bound()) {
    return;
  }

  const time_t now = time(nullptr);
  if (now < 1700000000) {
    return;
  }

  const time_t preWakeAt =
      phoneAlarmEpoch - (time_t)preWakeMinutes * 60;
  if (preWakeEnabled && !phoneAlarmPrewakeFired &&
      now >= preWakeAt && now < phoneAlarmEpoch) {
    phoneAlarmPrewakeFired = true;
    triggerPreWake();
  }

  if (!phoneAlarmWakeFired && now >= phoneAlarmEpoch &&
      now <= phoneAlarmEpoch + 300) {
    phoneAlarmWakeFired = true;
    triggerWakeLight(true, "synced phone alarm");
  }

  if (now > phoneAlarmEpoch + 300) {
    phoneAlarmEpoch = 0;
    phoneAlarmPrewakeFired = false;
    phoneAlarmWakeFired = false;
    preferences.putULong64("phoneAlarm", 0);
  }
}

void handleOffButton() {
  const bool reading = digitalRead(OFF_BUTTON_PIN);
  const uint32_t now = millis();
  if (reading != buttonLastReading) {
    buttonLastReading = reading;
    buttonLastChanged = now;
  }

  if (reading != buttonStableState && now - buttonLastChanged >= BUTTON_DEBOUNCE_MS) {
    buttonStableState = reading;
    if (buttonStableState == LOW && zbSwitch.bound()) {
      wakePhase = WakePhase::IDLE;
      wakeRoutineRequiresPhone = false;
      currentPower = false;
      sendLightOff();
      Serial.println("Bulb turned off with the ESP32 BOOT button.");
    }
  }
}

void handleSerial() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command == "on") {
    if (currentBrightness == 0) currentBrightness = 50;
    applyLight();
  } else if (command == "off") {
    wakePhase = WakePhase::IDLE;
    wakeRoutineRequiresPhone = false;
    sendLightOff();
    currentPower = false;
  } else if (command == "toggle") {
    sendLightToggle();
    currentPower = !currentPower;
  } else if (command == "pair") {
    Zigbee.openNetwork(PAIRING_SECONDS);
    lastPairingOpen = millis();
    Serial.println("Pairing open for 180 seconds.");
  } else if (command == "devices") {
    zbSwitch.printBoundDevices(Serial);
  } else if (command == "help") {
    Serial.println("Commands: on, off, toggle, pair, devices, help");
  } else {
    Serial.println("Unknown command. Type help.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  pixel.begin();
  pinMode(OFF_BUTTON_PIN, INPUT_PULLUP);
  preferences.begin("wake-light", false);
  alarmEnabled = preferences.getBool("alarmOn", false);
  alarmHour = preferences.getUChar("alarmHour", 6);
  alarmMinute = preferences.getUChar("alarmMinute", 30);
  preWakeEnabled = preferences.getBool("preOn", false);
  preWakeMinutes = preferences.getUChar("preMin", 10);
  preWakeBrightness = preferences.getUChar("preBri", 10);
  preWakeKelvin = preferences.getUShort("preK", 2200);
  wakeBrightness = preferences.getUChar("wakeBri", 100);
  wakeKelvin = preferences.getUShort("wakeK", 4000);
  calmAfterMinutes = preferences.getUChar("calmMin", 10);
  calmBrightness = preferences.getUChar("calmBri", 35);
  calmKelvin = preferences.getUShort("calmK", 2700);
  offAfterMinutes = preferences.getUChar("offMin", 20);
  phoneAbsenceMinutes = preferences.getUChar("absMin", 5);
  phoneAlarmEpoch = (time_t)preferences.getULong64("phoneAlarm", 0);
  lastTriggerEpoch = (time_t)preferences.getULong64("lastTrigAt", 0);
  lastTriggerSource = preferences.getString("lastTrig", "none");
  setStatusLedMode(StatusLedMode::WIFI_CONNECTING);
  updateStatusPixel();

  startWiFi();
  setStatusLedMode(StatusLedMode::ZIGBEE_STARTING);
  updateStatusPixel();
  esp_coex_wifi_i154_enable();

  zbSwitch.setManufacturerAndModel("Daniel", "ESP32-C6 IKEA controller");
  zbSwitch.allowMultipleBinding(false);
  Zigbee.addEndpoint(&zbSwitch);
  Zigbee.setRebootOpenNetwork(PAIRING_SECONDS);

  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start. Restarting...");
    setStatusLedMode(StatusLedMode::ERROR);
    const uint32_t errorStarted = millis();
    while (millis() - errorStarted < 2000) {
      updateStatusPixel();
      delay(10);
    }
    ESP.restart();
  }

  configTzTime(TZ_COPENHAGEN, "pool.ntp.org", "time.cloudflare.com");
  configureWebServer();
  lastPairingOpen = millis();
  setStatusLedMode(zbSwitch.bound() ? StatusLedMode::PAIRED : StatusLedMode::PAIRING);
  Serial.println("Ready. Pairing stays open until a bulb binds.");
}

void loop() {
  server.handleClient();
  handleSerial();
  handleOffButton();
  checkAlarm();
  checkPhoneAlarm();
  checkWakeRoutine();

  const bool bound = zbSwitch.bound();
  if (!bound && millis() - lastPairingOpen >= PAIRING_REOPEN_MS) {
    Zigbee.openNetwork(PAIRING_SECONDS);
    lastPairingOpen = millis();
    Serial.println("No bulb bound; Zigbee pairing reopened automatically.");
  }

  if (WiFi.status() != WL_CONNECTED && millis() - lastWiFiRetry >= WIFI_RETRY_MS) {
    lastWiFiRetry = millis();
    Serial.println("Retrying Wi-Fi...");
    WiFi.reconnect();
  }
  if (WiFi.status() == WL_CONNECTED && !mdnsStarted && MDNS.begin("wake-light")) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
    Serial.println("Stable local name: http://wake-light.local/");
  }

  if (bound != wasBound) {
    wasBound = bound;
    setStatusLedMode(bound ? StatusLedMode::PAIRED : StatusLedMode::PAIRING);
    Serial.println(bound ? "Bulb paired and bound." : "Bulb not bound.");
  }
  updateStatusPixel();
  delay(2);
}
