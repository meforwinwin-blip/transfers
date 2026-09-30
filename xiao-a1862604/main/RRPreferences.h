#pragma once

#include <Arduino.h>
#include "Configuration.h"

#if BOARD == BOARD_ESP32_FEATHER || BOARD == BOARD_ESP32_LOLIND32

  #include <Preferences.h>
  using RRPreferences = Preferences;

#else

  #include <Adafruit_LittleFS.h>
  #include <InternalFileSystem.h>

// Small Preferences-compatible wrapper for nRF52 targets.
//
// ESP32 targets continue to use their native Preferences/NVS implementation.
// nRF52 targets store each preference as a tiny LittleFS file in the internal
// flash region provided by the Seeed/Adafruit nRF52 core. Values therefore
// survive reset and complete power loss.
//
// The public methods intentionally mirror only the Preferences API currently
// used by RejsaRubberTrac.
class RRPreferences {
private:
  char _namespace[24] = {0};
  bool _readOnly = false;
  bool _begun = false;

  static bool ensureFileSystem() {
    static bool attempted = false;
    static bool mounted = false;

    if (!attempted) {
      attempted = true;
      mounted = InternalFS.begin();
    }
    return mounted;
  }

  static void sanitizeComponent(const char* input, char* output, size_t outputSize) {
    if (outputSize == 0) return;

    size_t j = 0;
    for (size_t i = 0; input && input[i] != '\0' && j + 1 < outputSize; ++i) {
      const char c = input[i];
      const bool safe =
          (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          c == '_' || c == '-';
      output[j++] = safe ? c : '_';
    }
    output[j] = '\0';
  }

  bool makePath(const char* key, char* path, size_t pathSize) const {
    if (!_begun || !_namespace[0] || !key || !key[0] || pathSize == 0) {
      return false;
    }

    char safeNamespace[24];
    char safeKey[40];
    sanitizeComponent(_namespace, safeNamespace, sizeof(safeNamespace));
    sanitizeComponent(key, safeKey, sizeof(safeKey));

    const int written = snprintf(
        path,
        pathSize,
        "/rrpref_%s_%s",
        safeNamespace,
        safeKey);

    return written > 0 && (size_t)written < pathSize;
  }

  bool readValue(const char* key, String& value) const {
    if (!ensureFileSystem()) return false;

    char path[80];
    if (!makePath(key, path, sizeof(path))) return false;

    Adafruit_LittleFS_Namespace::File file(InternalFS);
    if (!file.open(path, Adafruit_LittleFS_Namespace::FILE_O_READ)) {
      return false;
    }

    value = "";
    value.reserve(file.size());

    while (file.available()) {
      const int c = file.read();
      if (c < 0) break;
      value += (char)c;
    }

    file.close();
    return true;
  }

  size_t writeValue(const char* key, const String& value) {
    if (_readOnly || !ensureFileSystem()) return 0;

    char path[80];
    if (!makePath(key, path, sizeof(path))) return 0;

    Adafruit_LittleFS_Namespace::File file(InternalFS);
    if (!file.open(path, Adafruit_LittleFS_Namespace::FILE_O_WRITE)) {
      return 0;
    }

    // FILE_O_WRITE opens at EOF. Preferences semantics require replacement,
    // not append, so rewind and truncate before writing the new value.
    if (!file.seek(0) || !file.truncate(0)) {
      file.close();
      return 0;
    }

    const size_t expected = value.length();
    const size_t written = expected == 0
        ? 0
        : file.write((const uint8_t*)value.c_str(), expected);

    file.flush();
    file.close();

    return written == expected ? expected : 0;
  }

public:
  bool begin(const char* name, bool readOnly = false) {
    if (!name || !name[0]) return false;
    if (!ensureFileSystem()) return false;

    strncpy(_namespace, name, sizeof(_namespace) - 1);
    _namespace[sizeof(_namespace) - 1] = '\0';
    _readOnly = readOnly;
    _begun = true;
    return true;
  }

  void end() {
    _namespace[0] = '\0';
    _readOnly = false;
    _begun = false;
  }

  int getInt(const char* key, int defaultValue = 0) {
    String value;
    if (!readValue(key, value)) return defaultValue;
    return value.toInt();
  }

  bool getBool(const char* key, bool defaultValue = false) {
    String value;
    if (!readValue(key, value)) return defaultValue;

    value.trim();
    value.toLowerCase();

    if (value == "1" || value == "true" || value == "on") return true;
    if (value == "0" || value == "false" || value == "off") return false;
    return defaultValue;
  }

  String getString(const char* key, const char* defaultValue = "") {
    String value;
    if (!readValue(key, value)) return String(defaultValue);
    return value;
  }

  size_t putInt(const char* key, int value) {
    const String encoded(value);
    return writeValue(key, encoded) == encoded.length() ? sizeof(value) : 0;
  }

  size_t putBool(const char* key, bool value) {
    const String encoded(value ? "1" : "0");
    return writeValue(key, encoded) == 1 ? 1 : 0;
  }

  size_t putString(const char* key, const String& value) {
    return writeValue(key, value);
  }
};

#endif

inline bool rrPrefsPutBoolChecked(RRPreferences& prefs, const char* key, bool value) {
  return prefs.putBool(key, value) > 0;
}

inline bool rrPrefsPutIntChecked(RRPreferences& prefs, const char* key, int value) {
  return prefs.putInt(key, value) > 0;
}

inline bool rrPrefsPutStringChecked(RRPreferences& prefs, const char* key, const String& value) {
  const size_t written = prefs.putString(key, value);
  if (value.length() > 0) return written > 0;

  // Both ESP32 Preferences and the LittleFS wrapper legitimately report zero
  // bytes for an empty string on some core versions. Verify the stored value
  // instead so mount/open failures are not reported as "Saved".
  return prefs.getString(key, "__RR_PREF_MISSING__") == "";
}
