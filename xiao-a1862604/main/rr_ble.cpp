#include "rr_ble.h"
#include "RRPreferences.h"
#include "Constants.h"

extern int vBattery;
extern int lipoPercentage;
extern float tempHistory[10];
extern int tempHistoryIdx;
extern bool irEnabled;
extern bool distEnabled;
extern int rgbMode;
extern bool wifiLowPower;
extern uint8_t mirrorTire;
extern float getHistoryAvg();
extern float getHistoryStDev();

char globalBleName[32] = "RejsaRubber";

BLDevice::BLDevice() { // We initialize a couple things in constructor
  datapackOne.distance = 0;
  datapackOne.protocol = PROTOCOL;
  datapackTwo.protocol = PROTOCOL;
  datapackThr.distance = 0;
  datapackThr.protocol = PROTOCOL;
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  mainService = BLEService(0x1FF7);
  GATTone = BLECharacteristic(0x01);
  GATTtwo = BLECharacteristic(0x02);
  GATTthr = BLECharacteristic(0x03);
#endif
}

#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
static BLEUart* gBleUart = nullptr;

void sendBleUartChunk(String msg) {
  if (!gBleUart) return;
  gBleUart->print(msg);
  delay(20);
}

void bleuart_rx_callback(uint16_t conn_hdl) {
  (void) conn_hdl;
  if (!gBleUart) return;

  String rawCommand = "";
  while (gBleUart->available()) {
    rawCommand += (char)gBleUart->read();
  }
  rawCommand.trim();
  if (rawCommand.length() == 0) return;

  Serial.printf("BLE UART Raw Received: %s\n", rawCommand.c_str());

  String cmdLower = rawCommand;
  cmdLower.toLowerCase();

  if (cmdLower.startsWith("set ")) {
    const int firstSpace = rawCommand.indexOf(' ');
    const int secondSpace = rawCommand.indexOf(' ', firstSpace + 1);
    if (secondSpace == -1) {
      gBleUart->println("Error: Expected 'set <key> <value>'");
      return;
    }

    String key = rawCommand.substring(firstSpace + 1, secondSpace);
    String value = rawCommand.substring(secondSpace + 1);
    key.toLowerCase();

    RRPreferences setPrefs;
    if (!setPrefs.begin(settingsNamespace, false)) {
      gBleUart->println("Error: settings storage unavailable");
      return;
    }

    bool valid = true;
    bool saved = true;

    if (key == "ir_enabled") {
      const bool e = (value == "on" || value == "1" || value == "true");
      saved = rrPrefsPutBoolChecked(setPrefs, "irEnabled", e);
      if (saved) {
        irEnabled = e;
        gBleUart->printf("Set ir_enabled: %s\n", e ? "on" : "off");
      }
    } else if (key == "dist_enabled") {
      const bool e = (value == "on" || value == "1" || value == "true");
      saved = rrPrefsPutBoolChecked(setPrefs, "distEnabled", e);
      if (saved) {
        distEnabled = e;
        gBleUart->printf("Set dist_enabled: %s\n", e ? "on" : "off");
      }
    } else if (key == "rgb_mode") {
      const int m = value.toInt();
      if (m >= 0 && m <= 4) {
        saved = rrPrefsPutIntChecked(setPrefs, "rgbMode", m);
        if (saved) {
          rgbMode = m;
          gBleUart->printf("Set rgb_mode: %d\n", m);
        }
      } else {
        gBleUart->println("Invalid rgb_mode. Use 0-4.");
        valid = false;
      }
    } else if (key == "mirror") {
      const bool e = (value == "on" || value == "1" || value == "true");
      saved = rrPrefsPutBoolChecked(setPrefs, "mirror", e);
      if (saved) {
        mirrorTire = e ? 1 : 0;
        gBleUart->printf("Set mirror: %s\n", e ? "on" : "off");
      }
    } else if (key == "wheel_pos") {
      String pos = value;
      pos.trim();
      pos.toUpperCase();
      if (pos == "FL" || pos == "FR" || pos == "RL" || pos == "RR") {
        saved = rrPrefsPutStringChecked(setPrefs, "wheelPos", pos);
        if (saved) gBleUart->printf("Set wheel_pos: %s (restart required)\n", pos.c_str());
      } else {
        gBleUart->println("Invalid wheel_pos. Use FL, FR, RL or RR.");
        valid = false;
      }
    } else if (
        key == "webserver" || key == "wifi_ssid" || key == "wifi_password" ||
        key == "wifi_mode" || key == "wifi_low_power" ||
        key == "ap_ssid" || key == "ap_password") {
      gBleUart->printf("Error: '%s' is not supported on nRF52 (no Wi-Fi).\n", key.c_str());
      valid = false;
    } else {
      gBleUart->printf("Unknown key: %s\n", key.c_str());
      valid = false;
    }

    setPrefs.end();

    if (valid && saved) gBleUart->println("Saved.");
    else if (valid && !saved) gBleUart->println("Error: settings write failed");

  } else if (cmdLower == "show all") {
    RRPreferences showPrefs;
    if (!showPrefs.begin(settingsNamespace, true)) {
      gBleUart->println("Error: settings storage unavailable");
      return;
    }

    sendBleUartChunk("--- All Settings ---\n");
    gBleUart->printf("wheel_pos:    %s\n", showPrefs.getString("wheelPos", DEFAULT_WHEEL_POS).c_str());
    gBleUart->printf("mirror:       %s\n", showPrefs.getBool("mirror", MIRRORTIRE == 1) ? "on" : "off");
    gBleUart->printf("rgb_mode:     %d\n", showPrefs.getInt("rgbMode", 1));
    gBleUart->printf("ir_enabled:   %s\n", showPrefs.getBool("irEnabled", true) ? "on" : "off");
    gBleUart->printf("dist_enabled: %s\n", showPrefs.getBool("distEnabled", true) ? "on" : "off");
    sendBleUartChunk("--------------------\n");
    showPrefs.end();

  } else if (cmdLower == "show temps") {
    sendBleUartChunk("--- Current Temperatures ---\n");
    for (int i = 0; i < FIS_X; i++) {
      gBleUart->printf("Bar %02d: %.1f C\n", i, (float)tempSensor.measurement[i] / 10.0);
      if (i % 4 == 0) delay(20);
    }
    sendBleUartChunk("----------------------------\n");

  } else if (cmdLower == "status" || cmdLower == "get status") {
    RRPreferences getPrefs;
    if (!getPrefs.begin(settingsNamespace, true)) {
      gBleUart->println("Error: settings storage unavailable");
      return;
    }
    const String wp = getPrefs.getString("wheelPos", DEFAULT_WHEEL_POS);
    getPrefs.end();

    sendBleUartChunk("--- Status ---\n");
    gBleUart->printf("Uptime: %s\n", getUptimeString().c_str());
    uint8_t mac[6];
    Bluefruit.getAddr(mac);
    gBleUart->printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    gBleUart->printf("RGB LED Mode: %d\n", rgbMode);
    gBleUart->printf("Wheel Pos: %s\n", wp.c_str());
    gBleUart->printf("Battery: %dmV (%d%%)\n", vBattery, lipoPercentage);
    gBleUart->printf("Avg Temp: %.1f C\n", getHistoryAvg());
    sendBleUartChunk("--------------\n");

  } else if (cmdLower.startsWith("get ")) {
    String key = rawCommand.substring(4);
    key.trim();
    key.toLowerCase();

    if (
        key == "webserver" || key == "wifi_ssid" || key == "wifi_password" ||
        key == "wifi_mode" || key == "wifi_low_power" ||
        key == "ap_ssid" || key == "ap_password") {
      gBleUart->printf("Error: '%s' is not supported on nRF52 (no Wi-Fi).\n", key.c_str());
      return;
    }

    RRPreferences getPrefs;
    if (!getPrefs.begin(settingsNamespace, true)) {
      gBleUart->println("Error: settings storage unavailable");
      return;
    }

    if (key == "ir_enabled") gBleUart->printf("ir_enabled: %s\n", getPrefs.getBool("irEnabled", true) ? "on" : "off");
    else if (key == "dist_enabled") gBleUart->printf("dist_enabled: %s\n", getPrefs.getBool("distEnabled", true) ? "on" : "off");
    else if (key == "rgb_mode") gBleUart->printf("rgb_mode: %d\n", getPrefs.getInt("rgbMode", 1));
    else if (key == "mirror") gBleUart->printf("mirror: %s\n", getPrefs.getBool("mirror", MIRRORTIRE == 1) ? "on" : "off");
    else if (key == "wheel_pos") gBleUart->printf("wheel_pos: %s\n", getPrefs.getString("wheelPos", DEFAULT_WHEEL_POS).c_str());
    else gBleUart->printf("Unknown key: %s\n", key.c_str());
    getPrefs.end();

  } else if (cmdLower == "webserver on" || cmdLower == "webserver off") {
    gBleUart->println("Error: webserver is not supported on nRF52 (no Wi-Fi).");

  } else if (cmdLower == "reset") {
    gBleUart->print("Resetting...\n");
    delay(500);
    NVIC_SystemReset();

  } else if (cmdLower == "help" || cmdLower == "?") {
    sendBleUartChunk("\nCommands:\n");
    sendBleUartChunk("  status, show all, show temps\n");
    sendBleUartChunk("  get <key>, set <key> <val>\n");
    sendBleUartChunk("  reset, help\n");
    sendBleUartChunk("\nKeys: wheel_pos, mirror, rgb_mode,\n");
    sendBleUartChunk("  ir_enabled, dist_enabled\n");
  } else {
    gBleUart->print("Unknown command. Type 'help'.\n");
  }
}
#endif

void BLDevice::setupMainService(void) {
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  mainService.begin();

  GATTone.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);  
  GATTone.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  GATTone.setFixedLen(20);
  GATTone.begin();

  GATTtwo.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ); 
  GATTtwo.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  GATTtwo.setFixedLen(20);
  GATTtwo.begin();

  GATTthr.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ); 
  GATTthr.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  GATTthr.setFixedLen(20);
  GATTthr.begin();

  bleuart.begin();
  gBleUart = &bleuart;
  bleuart.setRxCallback(bleuart_rx_callback);
#elif BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  mainService = mainServer->createService(BLE_SERVICE_UUID);

  GATTone = mainService->createCharacteristic(GATT_ONE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  GATTone->addDescriptor(new BLE2902());

  GATTtwo = mainService->createCharacteristic(GATT_TWO_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  GATTtwo->addDescriptor(new BLE2902());

  GATTthr = mainService->createCharacteristic(GATT_THR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  GATTthr->addDescriptor(new BLE2902());

  mainService->start();

  pUartService = mainServer->createService(UART_SERVICE_UUID);
  pTxCharacteristic = pUartService->createCharacteristic(UART_TX_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  pRxCharacteristic = pUartService->createCharacteristic(UART_RX_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new UARTCallbacks(pTxCharacteristic));
  mainServer->setCallbacks(new MyServerCallbacks(pTxCharacteristic));
  pUartService->start();
#endif
}

void BLDevice::startAdvertising(void) {
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(mainService);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    
  Bluefruit.Advertising.setFastTimeout(30);      
  Bluefruit.Advertising.start(0);                
#elif BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  mainAdvertising = mainServer->getAdvertising();
  mainAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  mainAdvertising->addServiceUUID(UART_SERVICE_UUID);
  mainAdvertising->setScanResponse(true);
  mainAdvertising->setMinPreferred(0x06);  
  mainAdvertising->setMinPreferred(0x12);
  mainAdvertising->start();
#endif
}

void BLDevice::setupDevice(char bleName[]) {
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  uint8_t macaddr[6];
  Bluefruit.autoConnLed(false);
  Bluefruit.begin();
  Bluefruit.getAddr(macaddr);
  char baseName[32];
  strncpy(baseName, bleName, sizeof(baseName) - 1);
  baseName[sizeof(baseName) - 1] = '\0';
  snprintf(bleName, 32, "%s%02X%02X%02X", baseName, macaddr[2], macaddr[1], macaddr[0]);
  strncpy(deviceName, bleName, 31);
  deviceName[31] = '\0';
  strncpy(globalBleName, bleName, 31);
  globalBleName[31] = '\0';
  Bluefruit.setName(bleName);
  Serial.print("Starting bluetooth with MAC address ");
  Serial.println();
#elif BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  strncpy(deviceName, bleName, 31);
  strncpy(globalBleName, bleName, 31);
  BLEDevice::init(bleName);
  mainServer = BLEDevice::createServer();
#endif
  Serial.printf("Device name: %s\n", bleName);

  setupMainService();
  startAdvertising();
}

void BLDevice::renderPacketTemperature(int16_t measurements[], uint8_t mirrorTire, one_t &FirstPacket, two_t &SecondPacket, thr_t &ThirdPacket) {
  for(uint8_t x=0;x<8;x++){
    FirstPacket.temps[x]=measurements[x*2];
    SecondPacket.temps[x]=measurements[x*2 + 1];
    ThirdPacket.temps[x]=max(FirstPacket.temps[x], SecondPacket.temps[x]);
  }
}

void BLDevice::renderPacketBattery(int vbattery, int percentage, two_t &SecondPacket) {
  SecondPacket.voltage = vbattery;
  SecondPacket.charge = percentage;
}

void BLDevice::sendPackets(one_t &FirstPacket, two_t &SecondPacket, thr_t &ThirdPacket) {
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  GATTone.notify(&FirstPacket, sizeof(one_t));
  GATTtwo.notify(&SecondPacket, sizeof(two_t));
  GATTthr.notify(&ThirdPacket, sizeof(thr_t));
#elif BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  GATTone->setValue((uint8_t*)&FirstPacket, sizeof(one_t));
  GATTtwo->setValue((uint8_t*)&SecondPacket, sizeof(two_t));
  GATTthr->setValue((uint8_t*)&ThirdPacket, sizeof(thr_t));
  GATTone->notify();
  GATTtwo->notify();
  GATTthr->notify();
#endif 
}

String BLDevice::getMacAddress() {
  uint8_t macaddr[6];
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  Bluefruit.getAddr(macaddr);
#elif BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  WiFi.macAddress(macaddr);
#endif
  char buf[20];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
  return String(buf);
}

boolean BLDevice::isConnected() {
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  return Bluefruit.connected();
#elif BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  int32_t connectedCount;
  connectedCount = mainServer->getConnectedCount();
  return (connectedCount > 0);
#endif
}

void BLDevice::transmit(int16_t tempMeasurements[], uint8_t mirrorTire, int16_t distance, int vBattery, int lipoPercentage) {
  renderPacketTemperature(tempMeasurements, mirrorTire, datapackOne, datapackTwo, datapackThr);
  renderPacketBattery(vBattery, lipoPercentage, datapackTwo);
  datapackOne.distance = datapackThr.distance = distance;
  sendPackets(datapackOne, datapackTwo, datapackThr);
}
