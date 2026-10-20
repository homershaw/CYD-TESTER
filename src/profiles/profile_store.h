#pragma once
#include <Arduino.h>
#include <vector>
#include "profile.h"

// Profiles live in /profiles (stored byte-for-byte as uploaded), sims in /sims.
// Loads the devices listed in unit.toml.
namespace profiles {
struct LoadedDevice {
  uint8_t unitId = 0;
  bool enabled = true;
  String file;
  profile::Status status = profile::Status::Error;
  String message;  // error / not-Modbus reason
  profile::Profile profile;
};

// Writes the built-in test_bench profile and sim if they are missing (never overwrites).
void installBuiltins();
// Parses every device in unit.toml. Returns the number that loaded.
size_t loadDevices();
const std::vector<LoadedDevice>& devices();
}  // namespace profiles
