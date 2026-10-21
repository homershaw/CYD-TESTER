#pragma once
#include <Arduino.h>
#include <vector>

// /config/unit.toml: the unit's own settings. Defaults are compiled in and
// written to LittleFS on first boot.
namespace unitcfg {
constexpr const char* Path = "/config/unit.toml";
constexpr const char* RedactedPassword = "********";

struct Wifi {
  String ssid = "modbus-test";
  String password = "riotwifi";
  bool staticIp = false;
  String ip = "192.168.1.50", netmask = "255.255.255.0", gateway = "192.168.1.1", dns = "192.168.1.1";
  uint16_t connectTimeoutS = 20;
  uint16_t portalAfterS = 120;
};
struct Modbus {
  uint16_t port = 502;
  uint8_t maxClients = 4;
  bool respondToAnyUnitId = false;
  bool unmappedReadZero = false;  // false = exception 02
  bool strictFunctionCodes = true;
  uint16_t idleTimeoutS = 60;
};
struct Device {
  uint8_t unitId = 1;
  String profile, sim;
  bool enabled = true;
};
struct Display {
  uint8_t rotation = 1;
  uint8_t brightness = 80;
  uint32_t sleepAfterS = 0;
};
struct Config {
  Wifi wifi;
  String apPassword;
  Modbus modbus;
  std::vector<Device> devices;
  String adminPassword;
  Display display;
};

Config defaults();
// Parses TOML text. Unknown keys and bad values become warnings, and the default is kept.
bool parse(const String& text, Config& out, String& error, std::vector<String>& warnings);
String toToml(const Config& c, bool redactPasswords);

// The live configuration; load() reads the file or creates it from defaults.
Config& current();
bool load(String& error, std::vector<String>& warnings);
bool save();                  // writes current()
bool save(const Config& c);   // writes c without changing current()
}  // namespace unitcfg
