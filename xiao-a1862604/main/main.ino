#include <Arduino.h>
#if defined(ARDUINO_Seeed_XIAO_nRF52840)
  #include <Adafruit_TinyUSB.h>
#endif
#include <Wire.h>
#include <Tasker.h>

#include "Configuration.h"
#include "RRPreferences.h"
#include "temp_sensor.h"
#include "dist_sensor.h"
#include "display.h"
#include "rr_ble.h"
#include "algo.h"
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  #include "web.h"
#endif
#include "BatteryMonitor.h"
  
TempSensor tempSensor;
DistSensor distSensor;
BatteryMonitor<> batteryMonitor;
uint8_t mirrorTire = 0;
bool distEnabled = true;
bool irEnabled = true;
int rgbMode = 1; // 0: Disabled, 1: Data, 2: Distance, 3: Temperature, 4: Battery
int rgbPulse = 0; // Current brightness for pulse effect
bool wifiLowPower = true;
char wheelPos[] = "  ";  // Wheel position for Tire A
char deviceNameSuffix[] = "  ";

#if BOARD == BOARD_ESP32_LOLIND32
  #if FIS_SENSOR2_PRESENT == 1
    TempSensor tempSensor2;
    uint8_t mirrorTire2 = 0;
    char wheelPos2[] = "  ";  // Wheel position for Tire B
  #endif
  #if DIST_SENSOR2 != DIST_NONE
    DistSensor distSensor2;
  #endif
#endif

BLDevice bleDevice;
Display display;
Tasker tasker;

const char* settingsNamespace = "rejsarubber";

int vBattery = 0;          // Current battery voltage in mV
int rawVBattery = 0;       // Raw (non-smoothed) battery voltage in mV
int lipoPercentage = 0;    // Current battery percentage
int uiRefreshRateMs = 250; // Default UI refresh rate for web dashboard
float updateRate = 0.0;    // Reflects the actual update rate
unsigned long thermalMeasureTime = 0; // Tracks duration of thermal sensor measurement
int measurementCycles = 0; // Counts how many measurement cycles were completed. printStatus() uses it to roughly calculate refresh rate.

float tempHistory[10] = {0,0,0,0,0,0,0,0,0,0};
int tempHistoryIdx = 0;

// Function declarations
void updateWheelPos(void);
void printStatus(void);
void blinkOnTempChange(int16_t);
void blinkOnDistChange(uint16_t);
void updateBattery(void);
void updateRefreshRate(void);
void updateTempHistory(void);
void updateRgbLed(void);
void setStatusRgb(uint8_t r, uint8_t g, uint8_t b);
void processSerialCommand(String cmd);

#ifdef DUMMYDATA
  #include "dummydata.h"
#endif


String getUptimeString() {
  unsigned long upSecs = millis() / 1000;
  int h = upSecs / 3600;
  int m = (upSecs % 3600) / 60;
  int s = upSecs % 60;
  char buf[16];
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  return String(buf);
}

float getHistoryAvg() {
  float sum = 0;
  for (int i = 0; i < 10; i++) sum += tempHistory[i];
  return sum / 10.0;
}

float getHistoryStDev() {
  float avg = getHistoryAvg();
  float sq_sum = 0;
  for (int i = 0; i < 10; i++) sq_sum += pow(tempHistory[i] - avg, 2);
  return sqrt(sq_sum / 10.0);
}

void setStatusRgb(uint8_t r, uint8_t g, uint8_t b) {
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  rgbLedWrite(RGB_BUILTIN, r, g, b);
#elif BOARD == BOARD_NRF52_XIAO
  // XIAO's RGB LED is common-anode, so PWM is inverted.
  r = constrain(r, 0, 32);
  g = constrain(g, 0, 32);
  b = constrain(b, 0, 32);
  analogWriteResolution(8);
  analogWrite(LED_RED,   255 - map(r, 0, 32, 0, 255));
  analogWrite(LED_GREEN, 255 - map(g, 0, 32, 0, 255));
  analogWrite(LED_BLUE,  255 - map(b, 0, 32, 0, 255));
#else
  (void)r; (void)g; (void)b;
#endif
}

// ----------------------------------------

void setup(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.begin(115200);
  delay(500); // Give Serial some time to stabilize
#if BOARD == BOARD_NRF52_XIAO
  Serial.printf("\nBegin startup. Board: BOARD_NRF52_XIAO. Arduino: %d\n", ARDUINO);
#else
  Serial.printf("\nBegin startup. Board: BOARD_ESP32_FEATHER. Arduino: %d\n", ARDUINO);
#endif

  {
    RRPreferences showPrefs;
    showPrefs.begin(settingsNamespace, true);
    Serial.println("--- Current Configuration ---");
    Serial.printf("wheel_pos:      %s\n", showPrefs.getString("wheelPos", DEFAULT_WHEEL_POS).c_str());
    Serial.printf("mirror:         %s\n", showPrefs.getBool("mirror", MIRRORTIRE == 1) ? "ON" : "OFF");
    Serial.printf("rgb_mode:       %d\n", showPrefs.getInt("rgbMode", 1));
    Serial.printf("ir_enabled:     %s\n", showPrefs.getBool("irEnabled", true) ? "ON" : "OFF");
    Serial.printf("dist_enabled:   %s\n", showPrefs.getBool("distEnabled", true) ? "ON" : "OFF");
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
    Serial.printf("wifi_mode:      %s\n", showPrefs.getInt("wifi_mode", RR_WIFI_MODE_STA) == RR_WIFI_MODE_AP ? "AP" : "STA");
    Serial.printf("wifi_low_power: %s\n", showPrefs.getBool("wifiLowPower", true) ? "ON" : "OFF");
    Serial.printf("wifi_ssid:      %s\n", showPrefs.getString("wifi_ssid", "klopsik").c_str());
    Serial.printf("ap_ssid:        %s\n", showPrefs.getString("ap_ssid", "").c_str());
    Serial.printf("autozoom:       %s\n", showPrefs.getBool("autozoom", true) ? "ON" : "OFF");
    Serial.printf("ui_refresh:     %d ms\n", showPrefs.getInt("uiRefreshRate", 250));
#endif
    Serial.println("-----------------------------");
    showPrefs.end();
  }

  bool buttonPressed = false;
  if (BUTTON_PIN >= 0) {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    delay(10); // Debounce/settle
    if (digitalRead(BUTTON_PIN) == LOW) { // Button pressed to GND
      Serial.println("Boot button pressed!");
      buttonPressed = true;
    }
  }

#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  RRPreferences prefs;
  prefs.begin(settingsNamespace, false);
  bool startWeb = prefs.getBool("webserver", false);
  if (buttonPressed || startWeb) {
    setStatusRgb(0, 32, 0);  // Green
    int mode = prefs.getInt("wifi_mode", RR_WIFI_MODE_STA);
    String ssid = prefs.getString("wifi_ssid", "klopsik");
    String pass = prefs.getString("wifi_password", "parowka1234");
    String apSsid = prefs.getString("ap_ssid", "");
    String apPass = prefs.getString("ap_password", "");
    prefs.end();
    setupWiFi(ssid.c_str(), pass.c_str(), mode, apSsid.c_str(), apPass.c_str());
    setupWebServer(handleSettings);
    tasker.setInterval([]() {
      server.handleClient();
    }, 50);
  } else {
    prefs.end();
  }
#else
  (void)buttonPressed;
#endif

#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  Serial.printf("ESP32 IDF version: %s\n", esp_get_idf_version());
  analogReadResolution(12); //12 bits
  analogSetAttenuation(ADC_11db);  //For all pins
#endif

  if (GPIODISTSENSORXSHUT >= 0) pinMode(GPIODISTSENSORXSHUT, OUTPUT);
  if (GPIOLEFT >= 0) pinMode(GPIOLEFT, INPUT_PULLUP);
  if (GPIOFRONT >= 0) pinMode(GPIOFRONT, INPUT_PULLUP);
  if (GPIOCAR >= 0) pinMode(GPIOCAR, INPUT_PULLUP);
#if BOARD == BOARD_ESP32_LOLIND32
  pinMode(GPIOUNUSEDA2, INPUT);
#endif

  batteryMonitor.setup(VBAT_PIN);
  updateBattery();
  updateWheelPos();
  char bleName[32];
  snprintf(bleName, sizeof(bleName), "RejsaRubber%s", deviceNameSuffix);

  #if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
    Wire.begin(GPIOSDA,GPIOSCL); 
  #else
    Wire.begin();
  #endif

  #if DIST_SENSOR != DIST_NONE
    if (distEnabled) {
      debug("Starting distance sensor for %s...\n", wheelPos);
      if (distSensor.initialise(&Wire, wheelPos)) {
        debug("Distance sensor for %s present.\n", wheelPos);
      } else {
        debug("ERROR: Distance sensor for %s not present.\n", wheelPos);
      }
    }
  #endif

  if (irEnabled) {
    debug("Starting temperature sensor for %s...\n", wheelPos);
    if (!tempSensor.initialise(FIS_REFRESHRATE, &Wire)) {
      debug("MCU Rebooting...\n");
      #if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
        ESP.restart();
      #elif BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
        NVIC_SystemReset();
      #endif
    }
  }

#if DISP_DEVICE != DISP_NONE
  display.setup();
  tasker.setInterval(updateDisplay,200);
#endif

  bleDevice.setupDevice(bleName);

#ifdef _DEBUG
  tasker.setInterval(printStatus, SERIAL_UPDATERATE*1000); 
#endif
  tasker.setInterval(updateTempHistory, 1000); 
  tasker.setInterval(updateRgbLed, 100); 
  tasker.setInterval(updateBattery, BATTERY_UPDATERATE*1000); 
  tasker.setInterval(updateRefreshRate, 2000);

  debug("System Running!\n");
}                                                           

String inputBuffer = "";

void loop() {
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  if (WiFi.getMode() & WIFI_AP) dnsServer.processNextRequest();
#endif
  
  #if DIST_SENSOR != DIST_NONE
    if (distEnabled) distSensor.measure();
  #endif

  bool thermalFrameReady = false;

#if FIS_SENSOR == FIS_AMG8833
  // The AMG8833 produces at most FIS_REFRESHRATE fresh frames per second.
  // Do not repeatedly read and retransmit the same frame between sensor updates.
  static uint32_t lastThermalSampleMs = 0;
  const uint32_t thermalIntervalMs = 1000UL / FIS_REFRESHRATE;
  const uint32_t nowMs = millis();

  if (irEnabled && (lastThermalSampleMs == 0 ||
      (uint32_t)(nowMs - lastThermalSampleMs) >= thermalIntervalMs)) {
    lastThermalSampleMs = nowMs;
    const unsigned long _t0 = millis();
    thermalFrameReady = tempSensor.measure();
    thermalMeasureTime = millis() - _t0;

    if (!thermalFrameReady) {
      static uint32_t lastThermalErrorLogMs = 0;
      if ((uint32_t)(nowMs - lastThermalErrorLogMs) >= 1000UL) {
        debug("ERROR: AMG8833 frame read failed; stale frame not transmitted.\n");
        lastThermalErrorLogMs = nowMs;
      }
    }
  }
#else
  const unsigned long _t0 = millis();
  if (irEnabled) thermalFrameReady = tempSensor.measure();
  thermalMeasureTime = millis() - _t0;
#endif

  // Preserve the existing distance/battery-only behavior when IR acquisition
  // is disabled: the BLE protocol can still be transmitted without a new
  // thermal frame.
  if (!irEnabled) thermalFrameReady = true;

  if (thermalFrameReady) {
    measurementCycles++;
    if (bleDevice.isConnected()) {
      bleDevice.transmit(tempSensor.measurement_16, mirrorTire, distSensor.distance, vBattery, lipoPercentage);
    }
  }
  
  // Non-blocking Serial reader
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processSerialCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
      if (inputBuffer.length() > 100) inputBuffer = ""; // Safety truncate
    }
  }

  tasker.loop();
}

void processSerialCommand(String rawCommand) {
  rawCommand.trim();
  if (rawCommand.length() == 0) return;

  Serial.printf("CMD: '%s'\n", rawCommand.c_str());

  String cmdLower = rawCommand;
  cmdLower.toLowerCase();

  if (cmdLower.startsWith("set ")) {
    int firstSpace = rawCommand.indexOf(' ');
    int secondSpace = rawCommand.indexOf(' ', firstSpace + 1);
    if (secondSpace == -1) {
      Serial.println("Error: Expected 'set <key> <value>'");
      return;
    }

    String key = rawCommand.substring(firstSpace + 1, secondSpace);
    String value = rawCommand.substring(secondSpace + 1);
    key.toLowerCase();

#if BOARD == BOARD_NRF52_XIAO
    const bool networkKey =
        key == "webserver" || key == "wifi_ssid" || key == "wifi_password" ||
        key == "wifi_mode" || key == "wifi_low_power" ||
        key == "ap_ssid" || key == "ap_password";
    if (networkKey) {
      Serial.printf("Error: '%s' is not supported on XIAO nRF52840 (no Wi-Fi).\n", key.c_str());
      return;
    }
#endif

    RRPreferences setPrefs;
    if (!setPrefs.begin(settingsNamespace, false)) {
      Serial.println("Error: settings storage unavailable");
      return;
    }

    bool valid = true;
    bool saved = true;

    if (key == "webserver") {
      bool e = (value == "on" || value == "1" || value == "true");
      saved = rrPrefsPutBoolChecked(setPrefs, "webserver", e);
      if (saved) Serial.printf("Set webserver: %s\n", e ? "on" : "off");
    } else if (key == "wifi_ssid") {
      saved = rrPrefsPutStringChecked(setPrefs, "wifi_ssid", value);
      if (saved) Serial.printf("Set wifi_ssid: %s\n", value.c_str());
    } else if (key == "wifi_password") {
      if (value.length() > 0) {
        saved = rrPrefsPutStringChecked(setPrefs, "wifi_password", value);
        if (saved) Serial.println("Set wifi_password: OK");
      } else {
        Serial.println("Error: Password cannot be empty via serial");
        valid = false;
      }
    } else if (key == "wifi_mode") {
      int m = -1;
      if (value == "sta" || value == "0") m = RR_WIFI_MODE_STA;
      else if (value == "ap" || value == "1") m = RR_WIFI_MODE_AP;
      if (m != -1) {
        saved = rrPrefsPutIntChecked(setPrefs, "wifi_mode", m);
        if (saved) Serial.printf("Set wifi_mode: %s\n", m == RR_WIFI_MODE_AP ? "AP" : "STA");
      } else {
        Serial.println("Error: Use 'sta' or 'ap'");
        valid = false;
      }
    } else if (key == "ir_enabled") {
      bool e = (value == "on" || value == "1" || value == "true");
      saved = rrPrefsPutBoolChecked(setPrefs, "irEnabled", e);
      if (saved) {
        irEnabled = e;
        Serial.printf("Set ir_enabled: %s\n", e ? "on" : "off");
      }
    } else if (key == "dist_enabled") {
      bool e = (value == "on" || value == "1" || value == "true");
      saved = rrPrefsPutBoolChecked(setPrefs, "distEnabled", e);
      if (saved) {
        distEnabled = e;
        Serial.printf("Set dist_enabled: %s\n", e ? "on" : "off");
      }
    } else if (key == "ap_ssid") {
      saved = rrPrefsPutStringChecked(setPrefs, "ap_ssid", value);
      if (saved) Serial.printf("Set ap_ssid: %s\n", value.c_str());
    } else if (key == "ap_password") {
      if (value.length() > 0) {
        saved = rrPrefsPutStringChecked(setPrefs, "ap_password", value);
        if (saved) Serial.println("Set ap_password: OK");
      } else {
        Serial.println("Error: Password cannot be empty via serial");
        valid = false;
      }
    } else if (key == "rgb_mode") {
      int m = value.toInt();
      if (m >= 0 && m <= 4) {
        saved = rrPrefsPutIntChecked(setPrefs, "rgbMode", m);
        if (saved) {
          rgbMode = m;
          Serial.printf("Set rgb_mode: %d\n", m);
        }
      } else {
        Serial.println("Error: Use 0-4");
        valid = false;
      }
    } else if (key == "wifi_low_power") {
      bool e = (value == "on" || value == "1" || value == "true");
      saved = rrPrefsPutBoolChecked(setPrefs, "wifiLowPower", e);
      if (saved) {
        wifiLowPower = e;
        Serial.printf("Set wifi_low_power: %s\n", e ? "on" : "off");
      }
    } else if (key == "mirror") {
      bool e = (value == "on" || value == "1" || value == "true");
      saved = rrPrefsPutBoolChecked(setPrefs, "mirror", e);
      if (saved) {
        mirrorTire = e ? 1 : 0;
        Serial.printf("Set mirror: %s\n", e ? "on" : "off");
      }
    } else if (key == "wheel_pos") {
      String pos = value;
      pos.trim();
      pos.toUpperCase();
      if (pos == "FL" || pos == "FR" || pos == "RL" || pos == "RR") {
        saved = rrPrefsPutStringChecked(setPrefs, "wheelPos", pos);
        if (saved) Serial.printf("Set wheel_pos: %s (restart required)\n", pos.c_str());
      } else {
        Serial.println("Error: Use FL, FR, RL or RR");
        valid = false;
      }
    } else {
      Serial.printf("Error: Unknown key '%s'\n", key.c_str());
      valid = false;
    }

    setPrefs.end();

    if (valid && saved) Serial.println("Saved.");
    else if (valid && !saved) Serial.println("Error: settings write failed");

  } else if (cmdLower == "show all") {
    RRPreferences showPrefs;
    if (!showPrefs.begin(settingsNamespace, true)) {
      Serial.println("Error: settings storage unavailable");
      return;
    }

    Serial.println("--- All Settings ---");
    Serial.printf("wheel_pos:      %s\n", showPrefs.getString("wheelPos", DEFAULT_WHEEL_POS).c_str());
    Serial.printf("mirror:         %s\n", showPrefs.getBool("mirror", MIRRORTIRE == 1) ? "on" : "off");
    Serial.printf("rgb_mode:       %d\n", showPrefs.getInt("rgbMode", 1));
    Serial.printf("ir_enabled:     %s\n", showPrefs.getBool("irEnabled", true) ? "on" : "off");
    Serial.printf("dist_enabled:   %s\n", showPrefs.getBool("distEnabled", true) ? "on" : "off");
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
    Serial.printf("wifi_mode:      %s\n", showPrefs.getInt("wifi_mode", RR_WIFI_MODE_STA) == RR_WIFI_MODE_AP ? "ap" : "sta");
    Serial.printf("wifi_low_power: %s\n", showPrefs.getBool("wifiLowPower", true) ? "on" : "off");
    Serial.printf("wifi_ssid:      %s\n", showPrefs.getString("wifi_ssid", "klopsik").c_str());
    Serial.printf("ap_ssid:        %s\n", showPrefs.getString("ap_ssid", "").c_str());
    Serial.printf("autozoom:       %s\n", showPrefs.getBool("autozoom", true) ? "on" : "off");
    Serial.printf("ui_refresh:     %d ms\n", showPrefs.getInt("uiRefreshRate", 250));
#endif
    Serial.println("--------------------");
    showPrefs.end();

  } else if (cmdLower == "show temps") {
    Serial.println("--- Current Temperatures ---");
    for (int i = 0; i < FIS_X; i++) {
      Serial.printf("Bar %02d: %.1f C\n", i, (float)tempSensor.measurement[i] / 10.0);
    }
    Serial.println("----------------------------");

  } else if (cmdLower == "status" || cmdLower == "get status") {
    RRPreferences getPrefs;
    if (!getPrefs.begin(settingsNamespace, true)) {
      Serial.println("Error: settings storage unavailable");
      return;
    }

    String wp = getPrefs.getString("wheelPos", DEFAULT_WHEEL_POS);
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
    int wm = getPrefs.getInt("wifi_mode", RR_WIFI_MODE_STA);
    String ssid = getPrefs.getString("wifi_ssid", "klopsik");
    String apSsid = getPrefs.getString("ap_ssid", "");
#endif
    getPrefs.end();

    Serial.println("--- System Status ---");
    Serial.printf("Uptime: %s\n", getUptimeString().c_str());
    Serial.printf("MAC: %s\n", bleDevice.getMacAddress().c_str());
    Serial.printf("RGB Mode: %d\n", rgbMode);
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
    Serial.printf("WiFi Mode: %s\n", wm == RR_WIFI_MODE_AP ? "AP" : "STA");
    if (wm == RR_WIFI_MODE_AP) Serial.printf("AP SSID: %s\n", (apSsid.length() > 0) ? apSsid.c_str() : bleDevice.deviceName);
    else Serial.printf("WiFi SSID: %s\n", ssid.c_str());
#endif
    Serial.printf("Wheel Pos: %s\n", wp.c_str());
    Serial.printf("Battery: %dmV (%d%%)\n", vBattery, lipoPercentage);
    Serial.printf("Avg Temp (10s): %.1f C (StDev: %.2f)\n", getHistoryAvg(), getHistoryStDev());
    Serial.println("---------------------");

  } else if (cmdLower.startsWith("get ")) {
    String key = cmdLower.substring(4);
    key.trim();

#if BOARD == BOARD_NRF52_XIAO
    const bool networkKey =
        key == "webserver" || key == "wifi_ssid" || key == "wifi_password" ||
        key == "wifi_mode" || key == "wifi_low_power" ||
        key == "ap_ssid" || key == "ap_password";
    if (networkKey) {
      Serial.printf("Error: '%s' is not supported on XIAO nRF52840 (no Wi-Fi).\n", key.c_str());
      return;
    }
#endif

    RRPreferences getPrefs;
    if (!getPrefs.begin(settingsNamespace, true)) {
      Serial.println("Error: settings storage unavailable");
      return;
    }

    if (key == "webserver") Serial.printf("webserver: %s\n", getPrefs.getBool("webserver", false) ? "on" : "off");
    else if (key == "wifi_ssid") Serial.printf("wifi_ssid: %s\n", getPrefs.getString("wifi_ssid", "klopsik").c_str());
    else if (key == "wifi_password") Serial.println("wifi_password: ********");
    else if (key == "wifi_mode") Serial.printf("wifi_mode: %s\n", getPrefs.getInt("wifi_mode", RR_WIFI_MODE_STA) == RR_WIFI_MODE_AP ? "ap" : "sta");
    else if (key == "ir_enabled") Serial.printf("ir_enabled: %s\n", getPrefs.getBool("irEnabled", true) ? "on" : "off");
    else if (key == "dist_enabled") Serial.printf("dist_enabled: %s\n", getPrefs.getBool("distEnabled", true) ? "on" : "off");
    else if (key == "ap_ssid") Serial.printf("ap_ssid: %s\n", getPrefs.getString("ap_ssid", "").c_str());
    else if (key == "ap_password") Serial.println("ap_password: ********");
    else if (key == "rgb_mode") Serial.printf("rgb_mode: %d\n", getPrefs.getInt("rgbMode", 1));
    else if (key == "wifi_low_power") Serial.printf("wifi_low_power: %s\n", getPrefs.getBool("wifiLowPower", true) ? "on" : "off");
    else if (key == "mirror") Serial.printf("mirror: %s\n", getPrefs.getBool("mirror", MIRRORTIRE == 1) ? "on" : "off");
    else if (key == "wheel_pos") Serial.printf("wheel_pos: %s\n", getPrefs.getString("wheelPos", DEFAULT_WHEEL_POS).c_str());
    else Serial.printf("Error: Unknown key '%s'\n", key.c_str());
    getPrefs.end();

  } else if (cmdLower == "webserver on" || cmdLower == "webserver off") {
#if BOARD == BOARD_NRF52_XIAO
    Serial.println("Error: webserver is not supported on XIAO nRF52840 (no Wi-Fi).");
#else
    const bool enable = cmdLower.endsWith("on");
    RRPreferences p;
    if (!p.begin(settingsNamespace, false)) {
      Serial.println("Error: settings storage unavailable");
      return;
    }
    const bool saved = rrPrefsPutBoolChecked(p, "webserver", enable);
    p.end();
    if (saved) Serial.printf("Webserver: %s%s\n", enable ? "ON" : "OFF", enable ? " (Restart required)" : "");
    else Serial.println("Error: settings write failed");
#endif

  } else if (cmdLower == "reset") {
    Serial.println("Resetting...");
    delay(100);
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
    ESP.restart();
#else
    NVIC_SystemReset();
#endif

  } else if (cmdLower == "help" || cmdLower == "?") {
    Serial.println("\nCommands:");
    Serial.println("  status, show all, show temps, get <key>, set <key> <val>");
#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
    Serial.println("  webserver on/off, reset, help");
    Serial.println("\nKeys: wheel_pos, mirror, rgb_mode, ir_enabled, dist_enabled, wifi_mode, wifi_low_power, wifi_ssid, wifi_password, ap_ssid, ap_password");
#else
    Serial.println("  reset, help");
    Serial.println("\nKeys: wheel_pos, mirror, rgb_mode, ir_enabled, dist_enabled");
#endif
  } else {
    Serial.println("Error: Unknown command. Type 'help'.");
  }
}

void updateDisplay(void) {
  display.refreshDisplay(tempSensor.measurement, tempSensor.outerTireEdgePositionSmoothed, tempSensor.innerTireEdgePositionSmoothed, tempSensor.validAutozoomFrame, updateRate, distSensor.distance, lipoPercentage, bleDevice.isConnected());
}

void updateWheelPos(void) {
  RRPreferences prefs;
  prefs.begin(settingsNamespace, true); 
  String _wheelPos = prefs.getString("wheelPos", DEFAULT_WHEEL_POS);
  distEnabled = prefs.getBool("distEnabled", true);
  irEnabled = prefs.getBool("irEnabled", true);
  rgbMode = prefs.getInt("rgbMode", 1);
  wifiLowPower = prefs.getBool("wifiLowPower", true);
  uiRefreshRateMs = prefs.getInt("uiRefreshRate", 250);
  mirrorTire = prefs.getBool("mirror", MIRRORTIRE == 1) ? 1 : 0;
  prefs.end();

  Serial.printf("Position: %s, Mirror: %s\n", _wheelPos.c_str(), mirrorTire ? "Yes" : "No");
  strncpy(wheelPos, _wheelPos.c_str(), 3);
  strncpy(deviceNameSuffix, wheelPos, 3);
}

void printStatus(void) {
  debug("Rate: %.1fHz\tV: %dmV (%d%%)\tWheel: %s\tD: %imm\n", updateRate, vBattery, lipoPercentage, wheelPos, distSensor.distance);
}

void updateRefreshRate(void) {
  static long lastUpdate = 0;
  updateRate = (float)measurementCycles / (millis()-lastUpdate) * 1000;
  lastUpdate = millis();
  measurementCycles = 0;
}

#if DISP_DEVICE == DISP_NONE
void blinkOnDistChange(uint16_t distnew) {}
void blinkOnTempChange(int16_t tempnew) {}
#endif

void updateBattery(void) {
  float v, p;
  batteryMonitor.read(v, p);
  vBattery = round(v * 1000.0);
  rawVBattery = vBattery; 
  lipoPercentage = round(p);
}

void updateTempHistory() {
  if (irEnabled) {
    tempHistory[tempHistoryIdx] = tempSensor.movingAvgFrameTmp / 10.0;
    tempHistoryIdx = (tempHistoryIdx + 1) % 10;
  }
}

void updateRgbLed() {
  if (rgbMode == 0) {
    setStatusRgb(0, 0, 0);
  } else if (rgbMode == 1) {
    if (rgbPulse > 0) {
      setStatusRgb(0, rgbPulse, rgbPulse);
      rgbPulse -= 8;
      if (rgbPulse < 0) rgbPulse = 0;
    } else {
      setStatusRgb(0, 0, 0);
    }
  } else if (rgbMode == 2) {
    int d = distSensor.distance;
    if (d < 0) d = 0; if (d > 500) d = 500;
    int r, g;
    if (d < 250) { r = 32; g = map(d, 0, 250, 0, 32); }
    else { r = map(d, 250, 500, 32, 0); g = 32; }
    setStatusRgb(r, g, 0);
  } else if (rgbMode == 3) {
    float t = tempSensor.movingAvgFrameTmp / 10.0;
    int r = 0, g = 0, b = 0;
    if (t <= 20) { b = 32; }
    else if (t <= 30) { b = map(t * 10, 200, 300, 32, 0); g = map(t * 10, 200, 300, 0, 32); }
    else if (t <= 40) { g = 32; r = map(t * 10, 300, 400, 0, 32); }
    else if (t <= 50) { r = 32; g = map(t * 10, 400, 500, 32, 0); }
    else { r = 32; }
    setStatusRgb(r, g, b);
  } else if (rgbMode == 4) {
    int p = lipoPercentage;
    int r, g;
    if (p < 50) { r = 32; g = map(p, 0, 50, 0, 32); }
    else { r = map(p, 50, 100, 32, 0); g = 32; }
    setStatusRgb(r, g, 0);
  }
}
