#include "profile_store.h"
#include <LittleFS.h>
#include "unit_config.h"

// Embedded from data/ by board_build.embed_txtfiles (NUL-terminated).
extern const char builtinProfile[] asm("_binary_data_profiles_test_bench_toml_start");
extern const char builtinSim[] asm("_binary_data_sims_test_bench_sim_toml_start");

namespace profiles {
namespace {
std::vector<LoadedDevice> loaded;

void writeIfMissing(const char* dir, const char* path, const char* text) {
  if (!LittleFS.exists(dir)) LittleFS.mkdir(dir);
  if (LittleFS.exists(path)) return;
  File f = LittleFS.open(path, FILE_WRITE);
  if (!f) { Serial.printf("Could not create %s\n", path); return; }
  f.print(text);
  f.close();
  Serial.printf("Installed built-in %s\n", path);
}
}  // namespace

void installBuiltins() {
  writeIfMissing("/profiles", "/profiles/test_bench.toml", builtinProfile);
  writeIfMissing("/sims", "/sims/test_bench.sim.toml", builtinSim);
}

size_t loadDevices() {
  loaded.clear();
  size_t ok = 0;
  for (const auto& d : unitcfg::current().devices) {
    LoadedDevice dev;
    dev.unitId = d.unitId;
    dev.enabled = d.enabled;
    dev.file = d.profile;
    const String path = "/profiles/" + d.profile;
    File f = LittleFS.open(path, FILE_READ);
    if (!f) {
      dev.message = "profile file " + path + " not found";
    } else {
      String text = f.readString();
      f.close();
      const auto r = profile::parse(std::string_view(text.c_str(), text.length()), dev.profile);
      dev.status = r.status;
      dev.message = r.message.c_str();
      if (r.status == profile::Status::Ok) ++ok;
    }
    loaded.push_back(std::move(dev));
  }
  for (const auto& d : loaded) {
    if (d.status == profile::Status::Ok) {
      Serial.printf("Unit %u: %s (%s %s) %u tags, %u mapped, address %s, %u warning(s)%s\n", d.unitId, d.file.c_str(),
                    d.profile.vendor.c_str(), d.profile.model.c_str(), unsigned(d.profile.tags.size()),
                    unsigned(d.profile.mappedCount()), profile::toString(d.profile.addressMode),
                    unsigned(d.profile.warnings.size()), d.enabled ? "" : " [disabled]");
      for (const auto& w : d.profile.warnings) Serial.printf("    - %s\n", w.c_str());
    } else {
      Serial.printf("Unit %u: %s NOT loaded: %s\n", d.unitId, d.file.c_str(), d.message.c_str());
    }
  }
  return ok;
}

const std::vector<LoadedDevice>& devices() { return loaded; }
}  // namespace profiles
