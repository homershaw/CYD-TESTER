#include "api.h"
#include "../profiles/unit_config.h"
#include "web_server.h"

namespace api {
namespace {
constexpr size_t MaxConfigBytes = 8192;
uint32_t rebootAt = 0;

// Collects a request body into a NUL-terminated malloc buffer; the server free()s
// _tempObject when the request ends.
void collectBody(AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index == 0) {
    if (total > MaxConfigBytes) return;
    r->_tempObject = calloc(total + 1, 1);
  }
  if (r->_tempObject && index + len <= MaxConfigBytes) memcpy(static_cast<char*>(r->_tempObject) + index, data, len);
}

String warningsJson(const std::vector<String>& w) {
  String j = "[";
  for (size_t i = 0; i < w.size(); ++i) {
    if (i) j += ',';
    j += "\"" + web::jsonEscape(w[i]) + "\"";
  }
  return j + "]";
}

void putConfig(AsyncWebServerRequest* r) {
  if (!authorized(r)) return r->requestAuthentication();
  const char* body = static_cast<const char*>(r->_tempObject);
  if (!body || !*body) {
    r->send(400, "application/json", "{\"ok\":false,\"error\":\"Send unit.toml as the request body (max 8 KB)\"}");
    return;
  }
  unitcfg::Config next;
  String error;
  std::vector<String> warnings;
  if (!unitcfg::parse(String(body), next, error, warnings)) {
    r->send(400, "application/json", "{\"ok\":false,\"error\":\"" + web::jsonEscape(error) + "\"}");
    return;
  }
  // Spec 7: a redacted password on import keeps the current one. The running config is
  // left alone (this runs on AsyncTCP's task); the saved file takes effect after a reboot.
  const auto& cur = unitcfg::current();
  if (next.wifi.password == unitcfg::RedactedPassword) next.wifi.password = cur.wifi.password;
  if (next.apPassword == unitcfg::RedactedPassword) next.apPassword = cur.apPassword;
  if (next.adminPassword == unitcfg::RedactedPassword) next.adminPassword = cur.adminPassword;
  if (!unitcfg::save(next)) {
    r->send(500, "application/json", "{\"ok\":false,\"error\":\"Could not write /config/unit.toml\"}");
    return;
  }
  r->send(200, "application/json",
          "{\"ok\":true,\"restartRequired\":true,\"warnings\":" + warningsJson(warnings) + "}");
}
}  // namespace

bool authorized(AsyncWebServerRequest* r) {
  const String& pw = unitcfg::current().adminPassword;
  return !pw.length() || r->authenticate("admin", pw.c_str());
}

void registerRoutes(AsyncWebServer& server) {
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* r) {
    AsyncWebServerResponse* res = r->beginResponse(200, "application/toml", unitcfg::toToml(unitcfg::current(), true));
    res->addHeader("Content-Disposition", "inline; filename=unit.toml");
    r->send(res);
  });
  server.on("/api/config", HTTP_PUT, putConfig, nullptr, collectBody);
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authorized(r)) return r->requestAuthentication();
    r->send(202, "application/json", "{\"ok\":true,\"rebootingInMs\":500}");
    rebootAt = millis() + 500;  // let the response go out first
  });
}

void service() {
  if (rebootAt && int32_t(millis() - rebootAt) >= 0) ESP.restart();
}
}  // namespace api
