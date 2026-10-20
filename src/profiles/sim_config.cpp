#include "sim_config.h"
#include <algorithm>
#include <toml++/toml.hpp>

namespace sim {
const char* toString(Mode m) {
  switch (m) {
    case Mode::Static: return "static";
    case Mode::Ramp: return "ramp";
    case Mode::Sine: return "sine";
    case Mode::Random: return "random";
    case Mode::Step: return "step";
    case Mode::Toggle: return "toggle";
    case Mode::Counter: return "counter";
    case Mode::Echo: return "echo";
    case Mode::Ldr: return "ldr";
  }
  return "?";
}

const Entry* Config::find(std::string_view tag) const {
  for (const auto& e : entries)
    if (e.tag == tag) return &e;
  return nullptr;
}

namespace {
bool parseMode(const std::string& s, Mode& m) {
  static const std::pair<const char*, Mode> names[] = {
      {"static", Mode::Static}, {"ramp", Mode::Ramp}, {"sine", Mode::Sine}, {"random", Mode::Random}, {"step", Mode::Step},
      {"toggle", Mode::Toggle}, {"counter", Mode::Counter}, {"echo", Mode::Echo}, {"ldr", Mode::Ldr}};
  for (const auto& [n, v] : names)
    if (s == n) { m = v; return true; }
  return false;
}

bool number(const toml::table& t, const char* key, double& out) {
  if (auto v = t[key].value<double>()) { out = *v; return true; }  // toml++ converts integers too
  return false;
}
}  // namespace

bool parse(std::string_view text, const profile::Profile& p, Config& out, std::string& error) {
  out = Config{};
  auto result = toml::parse(text);
  if (!result) {
    error = "TOML syntax error at line " + std::to_string(result.error().source().begin.line) + ": " +
            std::string(result.error().description());
    return false;
  }
  const toml::table& root = result.table();
  for (const auto& [key, node] : root) {
    const std::string_view k = key.str();
    if (k != "schema_version" && k != "profile_id" && k != "sim") out.warnings.push_back("unknown key '" + std::string(k) + "' ignored");
  }
  out.profileId = root["profile_id"].value_or<std::string>("");
  if (out.profileId.empty()) { error = "profile_id is missing"; return false; }
  if (out.profileId != p.profileId)
    out.warnings.push_back("profile_id \"" + out.profileId + "\" does not match the profile (\"" + p.profileId + "\")");

  const toml::array* sims = root["sim"].as_array();
  if (!sims) return true;
  size_t index = 0;
  for (const auto& node : *sims) {
    std::string where = "sim[" + std::to_string(index++) + "]";
    const toml::table* t = node.as_table();
    if (!t) { out.warnings.push_back(where + " is not a table; skipped"); continue; }
    Entry e;
    e.tag = (*t)["tag"].value_or<std::string>("");
    if (e.tag.empty()) { out.warnings.push_back(where + " has no tag; skipped"); continue; }
    where = "sim '" + e.tag + "': ";
    const profile::Tag* tag = p.find(e.tag);
    if (!tag) { out.warnings.push_back(where + "no such tag in the profile; skipped"); continue; }
    if (out.find(e.tag)) { out.warnings.push_back(where + "duplicate entry; skipped"); continue; }
    for (const auto& [key, n] : *t) {
      const std::string_view k = key.str();
      static const char* known[] = {"tag", "mode", "initial", "min", "max", "period_s", "noise", "update_ms"};
      if (std::none_of(std::begin(known), std::end(known), [&](const char* x) { return k == x; }))
        out.warnings.push_back(where + "unknown key '" + std::string(k) + "' ignored");
    }
    const std::string mode = (*t)["mode"].value_or<std::string>("static");
    if (!parseMode(mode, e.mode)) { out.warnings.push_back(where + "unknown mode \"" + mode + "\"; using static"); e.mode = Mode::Static; }
    number(*t, "initial", e.initial);
    number(*t, "min", e.min);
    number(*t, "max", e.max);
    if (!std::isnan(e.min) && !std::isnan(e.max) && e.min > e.max) {
      std::swap(e.min, e.max);
      out.warnings.push_back(where + "min is above max; swapped");
    }
    double v;
    if (number(*t, "period_s", v)) {
      if (v > 0) e.periodS = v;
      else out.warnings.push_back(where + "period_s must be above 0; using 60");
    }
    if (number(*t, "noise", v)) e.noise = v < 0 ? -v : v;
    if (number(*t, "update_ms", v)) {
      if (v >= 50) e.updateMs = uint32_t(v);
      else out.warnings.push_back(where + "update_ms below 50; using 50");
      if (v < 50) e.updateMs = 50;
    }
    const bool needsRange = e.mode == Mode::Ramp || e.mode == Mode::Sine || e.mode == Mode::Random;
    if (needsRange && (std::isnan(e.min) || std::isnan(e.max)))
      out.warnings.push_back(where + std::string(toString(e.mode)) + " needs min and max; the value stays static");
    out.entries.push_back(e);
  }
  return true;
}

double startValue(const profile::Tag& t, const Entry* e) {
  double v = 0;
  if (e && !std::isnan(e->initial)) v = e->initial;
  else if (e && !std::isnan(e->min) && !std::isnan(e->max)) v = (e->min + e->max) / 2;
  if (t.type == profile::DataType::Bool) v = v != 0 ? 1 : 0;
  return v;
}
}  // namespace sim
