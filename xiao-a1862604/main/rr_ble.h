#ifndef RR_BLE_H
#define RR_BLE_H

#include "Configuration.h"
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  #include <bluefruit.h>
#elif BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  #include <BLEDevice.h>
  #include <BLEServer.h>
  #include <BLEUtils.h>
  #include <BLE2902.h>
  #include <WiFi.h>
#endif
#include <Arduino.h>
#include "protocol.h"
#include "RRPreferences.h"
#include "temp_sensor.h"
#include "Constants.h"

extern TempSensor tempSensor;

#define BLE_SERVICE_UUID           BLEUUID((uint16_t)0x1FF7)
#define GATT_ONE_UUID              BLEUUID((uint16_t)0x0001)
#define GATT_TWO_UUID              BLEUUID((uint16_t)0x0002)
#define GATT_THR_UUID              BLEUUID((uint16_t)0x0003)

#define UART_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_RX_CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_TX_CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern int vBattery;
extern int lipoPercentage;
extern String getUptimeString();
extern char globalBleName[32];
extern float tempHistory[10];
extern int tempHistoryIdx;
extern bool irEnabled;
extern bool distEnabled;
extern int rgbMode;
extern bool wifiLowPower;
extern uint8_t mirrorTire;
extern float getHistoryAvg();
extern float getHistoryStDev();

#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
class UARTCallbacks : public BLECharacteristicCallbacks {
    void sendBleChunk(String msg) {
        if (!_pTxCharacteristic) return;
        _pTxCharacteristic->setValue(msg.c_str());
        _pTxCharacteristic->notify();
        delay(20); // Small delay to allow the client to process the chunk
    }

    void onWrite(BLECharacteristic* pCharacteristic) {
        String rawValue = pCharacteristic->getValue().c_str();
        if (rawValue.length() > 0) {
            Serial.printf("BLE UART Raw Received: %s\n", rawValue.c_str()); 
            if (_pTxCharacteristic) {
                String rawStr = rawValue;
                rawStr.trim();
                if (rawStr.length() == 0) return;

                String cmdLower = rawStr;
                cmdLower.toLowerCase();

                if (cmdLower.startsWith("set ")) {
                    int firstSpace = rawStr.indexOf(' ');
                    int secondSpace = rawStr.indexOf(' ', firstSpace + 1);
                    if (secondSpace != -1) {
                        String key = rawStr.substring(firstSpace + 1, secondSpace);
                        String value = rawStr.substring(secondSpace + 1);
                        key.toLowerCase();
                        RRPreferences setPrefs;
                        setPrefs.begin(settingsNamespace, false);
                        bool valid = true;
                        String msg = "";
                        if (key == "webserver") {
                            bool e = (value == "on" || value == "1" || value == "true");
                            setPrefs.putBool("webserver", e);
                            msg = "Set webserver: " + String(e ? "on" : "off") + "\n";
                        } else if (key == "wifi_ssid") {
                            setPrefs.putString("wifi_ssid", value);
                            msg = "Set wifi_ssid: " + value + "\n";
                        } else if (key == "wifi_password") {
                            if (value.length() > 0) {
                                setPrefs.putString("wifi_password", value);
                                msg = "Set wifi_password: OK\n";
                            } else {
                                msg = "Error: Password cannot be empty\n";
                                valid = false;
                            }
                        } else if (key == "wifi_mode") {
                            int m = -1;
                            if (value == "sta" || value == "0") m = RR_WIFI_MODE_STA;
                            else if (value == "ap" || value == "1") m = RR_WIFI_MODE_AP;
                            if (m != -1) {
                                setPrefs.putInt("wifi_mode", m);
                                msg = "Set wifi_mode: " + String(m == RR_WIFI_MODE_AP ? "AP" : "STA") + "\n";
                            } else {
                                msg = "Invalid wifi_mode. Use sta or ap.\n";
                                valid = false;
                            }
                        } else if (key == "ir_enabled") {
                            bool e = (value == "on" || value == "1" || value == "true");
                            setPrefs.putBool("irEnabled", e);
                            irEnabled = e;
                            msg = "Set ir_enabled: " + String(e ? "on" : "off") + "\n";
                        } else if (key == "dist_enabled") {
                            bool e = (value == "on" || value == "1" || value == "true");
                            setPrefs.putBool("distEnabled", e);
                            distEnabled = e;
                            msg = "Set dist_enabled: " + String(e ? "on" : "off") + "\n";
                        } else if (key == "ap_ssid") {
                            setPrefs.putString("ap_ssid", value);
                            msg = "Set ap_ssid: " + value + "\n";
                        } else if (key == "ap_password") {
                            if (value.length() > 0) {
                                setPrefs.putString("ap_password", value);
                                msg = "Set ap_password: OK\n";
                            } else {
                                msg = "Error: Password cannot be empty\n";
                                valid = false;
                            }
                        } else if (key == "rgb_mode") {
                            int m = value.toInt();
                            if (m >= 0 && m <= 4) {
                                setPrefs.putInt("rgbMode", m);
                                rgbMode = m;
                                msg = "Set rgb_mode: " + String(m) + "\n";
                            } else {
                                msg = "Invalid rgb_mode. Use 0-4.\n";
                                valid = false;
                            }
                        } else if (key == "wifi_low_power") {
                            bool e = (value == "on" || value == "1" || value == "true");
                            setPrefs.putBool("wifiLowPower", e);
                            wifiLowPower = e;
                            msg = "Set wifi_low_power: " + String(e ? "on" : "off") + "\n";
                        } else if (key == "mirror") {
                            bool e = (value == "on" || value == "1" || value == "true");
                            setPrefs.putBool("mirror", e);
                            mirrorTire = e ? 1 : 0;
                            msg = "Set mirror: " + String(e ? "on" : "off") + "\n";
                        } else if (key == "wheel_pos") {
                            String pos = value;
                            pos.trim();
                            pos.toUpperCase();
                            if (pos == "FL" || pos == "FR" || pos == "RL" || pos == "RR") {
                                setPrefs.putString("wheelPos", pos);
                                msg = "Set wheel_pos: " + pos + " (restart required)\n";
                            } else {
                                msg = "Invalid wheel_pos. Use FL, FR, RL or RR.\n";
                                valid = false;
                            }
                        } else {
                            msg = "Unknown key: " + key + "\n";
                            valid = false;
                        }
                        setPrefs.end();
                        if (valid) msg += "Saved.\n";
                        sendBleChunk(msg);
                    }
                } else if (cmdLower == "show all") {
                    RRPreferences showPrefs;
                    showPrefs.begin(settingsNamespace, true);
                    sendBleChunk("--- All Settings ---\n");
                    sendBleChunk("wifi_mode:      " + String(showPrefs.getInt("wifi_mode", RR_WIFI_MODE_STA) == RR_WIFI_MODE_AP ? "ap" : "sta") + "\n");
                    sendBleChunk("wifi_low_power: " + String(showPrefs.getBool("wifiLowPower", true) ? "on" : "off") + "\n");
                    sendBleChunk("mirror:         " + String(showPrefs.getBool("mirror", MIRRORTIRE == 1) ? "on" : "off") + "\n");
                    sendBleChunk("rgb_mode:       " + String(showPrefs.getInt("rgbMode", 1)) + "\n");
                    sendBleChunk("ir_enabled:     " + String(showPrefs.getBool("irEnabled", true) ? "on" : "off") + "\n");
                    sendBleChunk("dist_enabled:   " + String(showPrefs.getBool("distEnabled", true) ? "on" : "off") + "\n");
                    sendBleChunk("wifi_ssid:      " + showPrefs.getString("wifi_ssid", "klopsik") + "\n");
                    sendBleChunk("ap_ssid:        " + showPrefs.getString("ap_ssid", "") + "\n");
                    sendBleChunk("autozoom:       " + String(showPrefs.getBool("autozoom", true) ? "on" : "off") + "\n");
                    sendBleChunk("ui_refresh:     " + String(showPrefs.getInt("uiRefreshRate", 250)) + " ms\n");
                    sendBleChunk("wheel_pos:      " + showPrefs.getString("wheelPos", DEFAULT_WHEEL_POS) + "\n");
                    sendBleChunk("--------------------\n");
                    showPrefs.end();
                } else if (cmdLower == "show temps") {
                    sendBleChunk("--- Temperatures ---\n");
                    String msg = "";
                    for (int i = 0; i < FIS_X; i++) {
                        msg += "Bar " + String(i < 10 ? "0" : "") + String(i) + ": " + String((float)tempSensor.measurement[i] / 10.0, 1) + " C\n";
                        if (msg.length() > 60) { 
                            sendBleChunk(msg);
                            msg = "";
                        }
                    }
                    if (msg.length() > 0) sendBleChunk(msg);
                    sendBleChunk("--------------------\n");
                } else if (cmdLower == "status" || cmdLower == "get status") {
                    RRPreferences getPrefs;
                    getPrefs.begin(settingsNamespace, true);
                    bool ws = getPrefs.getBool("webserver", false);
                    int wm = getPrefs.getInt("wifi_mode", RR_WIFI_MODE_STA);
                    String ssid = getPrefs.getString("wifi_ssid", "klopsik");
                    String apSsid = getPrefs.getString("ap_ssid", "");
                    String wp = getPrefs.getString("wheelPos", DEFAULT_WHEEL_POS);
                    getPrefs.end();
                    sendBleChunk("--- System Status ---\n");
                    sendBleChunk("Uptime: " + getUptimeString() + "\n");
                    sendBleChunk("MAC: " + WiFi.macAddress() + "\n");
                    sendBleChunk("RGB Mode: " + String(rgbMode) + "\n");
                    sendBleChunk("WiFi Mode: " + String(wm == RR_WIFI_MODE_AP ? "AP" : "STA") + "\n");
                    if (wm == RR_WIFI_MODE_AP) {
                        sendBleChunk("AP SSID: " + ((apSsid.length() > 0) ? apSsid : String(globalBleName)) + "\n");
                    } else {
                        sendBleChunk("WiFi SSID: " + ssid + "\n");
                    }
                    if (WiFi.status() == WL_CONNECTED || (WiFi.getMode() & WIFI_AP)) {
                        sendBleChunk("WiFi IP: " + ((wm == RR_WIFI_MODE_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\n");
                    } else {
                        sendBleChunk("WiFi IP: Not Connected\n");
                    }
                    sendBleChunk("Wheel Pos: " + wp + "\n");
                    sendBleChunk("Battery: " + String(vBattery) + "mV (" + String(lipoPercentage) + "%)\n");
                    sendBleChunk("Avg Temp: " + String(getHistoryAvg(), 1) + " C (StDev: " + String(getHistoryStDev(), 2) + ")\n");
                    sendBleChunk("--------------\n");
                } else if (cmdLower.startsWith("get ")) {
                    String key = rawStr.substring(4);
                    key.trim();
                    RRPreferences getPrefs;
                    getPrefs.begin(settingsNamespace, true);
                    if (key == "webserver") sendBleChunk(getPrefs.getBool("webserver", false) ? "webserver: on\n" : "webserver: off\n");
                    else if (key == "wifi_ssid") sendBleChunk("wifi_ssid: " + getPrefs.getString("wifi_ssid", "klopsik") + "\n");
                    else if (key == "wifi_password") sendBleChunk("wifi_password: ********\n");
                    else if (key == "wifi_mode") sendBleChunk(getPrefs.getInt("wifi_mode", RR_WIFI_MODE_STA) == RR_WIFI_MODE_AP ? "wifi_mode: ap\n" : "wifi_mode: sta\n");
                    else if (key == "ir_enabled") sendBleChunk(getPrefs.getBool("irEnabled", true) ? "ir_enabled: on\n" : "ir_enabled: off\n");
                    else if (key == "dist_enabled") sendBleChunk(getPrefs.getBool("distEnabled", true) ? "dist_enabled: on\n" : "dist_enabled: off\n");
                    else if (key == "ap_ssid") sendBleChunk("ap_ssid: " + getPrefs.getString("ap_ssid", "") + "\n");
                    else if (key == "ap_password") sendBleChunk("ap_password: ********\n");
                    else if (key == "rgb_mode") sendBleChunk("rgb_mode: " + String(getPrefs.getInt("rgbMode", 1)) + "\n");
                    else if (key == "wifi_low_power") sendBleChunk(getPrefs.getBool("wifiLowPower", true) ? "wifi_low_power: on\n" : "wifi_low_power: off\n");
                    else if (key == "mirror") sendBleChunk(getPrefs.getBool("mirror", MIRRORTIRE == 1) ? "mirror: on\n" : "mirror: off\n");
                    else if (key == "wheel_pos") sendBleChunk("wheel_pos: " + getPrefs.getString("wheelPos", DEFAULT_WHEEL_POS) + "\n");
                    else sendBleChunk("Unknown key: " + key + "\n");
                    getPrefs.end();
                } else if (cmdLower == "webserver on") {
                    RRPreferences p; p.begin(settingsNamespace, false); p.putBool("webserver", true); p.end();
                    sendBleChunk("Webserver: ON (Restart required)\n");
                } else if (cmdLower == "webserver off") {
                    RRPreferences p; p.begin(settingsNamespace, false); p.putBool("webserver", false); p.end();
                    sendBleChunk("Webserver: OFF\n");
                } else if (cmdLower == "reset") {
                    sendBleChunk("Resetting...\n");
                    delay(500);
                    ESP.restart();
                } else if (cmdLower == "help" || cmdLower == "?") {
                    sendBleChunk("\nCommands:\n");
                    sendBleChunk("  status, show all, show temps\n");
                    sendBleChunk("  get <key>, set <key> <val>\n");
                    sendBleChunk("  webserver on/off, reset, help\n");
                    sendBleChunk("\nKeys: wheel_pos, mirror, rgb_mode,\n");
                    sendBleChunk("  ir_enabled, dist_enabled, wifi_mode,\n");
                    sendBleChunk("  wifi_ssid, wifi_password,\n");
                    sendBleChunk("  ap_ssid, ap_password\n");
                } else {
                    sendBleChunk("Error: Unknown command. Type 'help'.\n");
                }
            }
        }
    }
    public:
        BLECharacteristic* _pTxCharacteristic; 
        UARTCallbacks(BLECharacteristic* pTx) : _pTxCharacteristic(pTx) {}
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("BLE Client Connected :-)");
        if (_pTxCharacteristic) {
            _pTxCharacteristic->setValue("Welcome to RejsaRubberTrac!\n");
            _pTxCharacteristic->notify();
            _pTxCharacteristic->setValue("Type 'help' for commands.\n");
            _pTxCharacteristic->notify();
        }
    }
    void onDisconnect(BLEServer* pServer) {
        Serial.println("BLE Client Disconnected");
    }
    public:
        BLECharacteristic* _pTxCharacteristic;
        MyServerCallbacks(BLECharacteristic* pTx) : _pTxCharacteristic(pTx) {}
};
#endif

class BLDevice {
private:
#if BOARD == BOARD_NRF52_FEATHER || BOARD == BOARD_NRF52_XIAO
  BLEService         mainService;
  BLECharacteristic  GATTone;
  BLECharacteristic  GATTtwo;
  BLECharacteristic  GATTthr;
  BLEUart            bleuart; 
#elif BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32
  BLEService*        mainService;
  BLEServer*         mainServer;
  BLEAdvertising*    mainAdvertising;
  BLECharacteristic* GATTone;
  BLECharacteristic* GATTtwo;
  BLECharacteristic* GATTthr;
  BLEService*        pUartService;
  BLECharacteristic* pTxCharacteristic;
  BLECharacteristic* pRxCharacteristic;
#endif
  one_t             datapackOne;
  two_t             datapackTwo;
  thr_t             datapackThr;
  void setupMainService(void);
  void startAdvertising(void);
  void renderPacketTemperature(int16_t measurements[], uint8_t mirrorTire, one_t &FirstPacket, two_t &SecondPacket, thr_t &ThirdPacket);
  void renderPacketBattery(int vbattery, int percentage, two_t &SecondPacket);
  void sendPackets(one_t &FirstPacket, two_t &SecondPacket, thr_t &ThirdPacket);
public:
  char deviceName[32];
  String getMacAddress();
  boolean isConnected();
  void setupDevice(char bleName[]);
  void transmit(int16_t tempMeasurements[], uint8_t mirrorTire, int16_t distance, int vBattery, int lipoPercentage);
  BLDevice();
};

#endif // RR_BLE_H
