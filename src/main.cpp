// RIOT Modbus TCP Test Unit (CYD-TESTER)
// Phase 2: Wi-Fi station, setup AP and captive portal, on top of the phase 1 board bring-up.
#include <Arduino.h>
#include <LittleFS.h>
#include "net/wifi_manager.h"
#include "profiles/profile_store.h"
#include "profiles/unit_config.h"
#include "ui/button.h"
#include "ui/display.h"
#include "ui/led.h"
#include "ui/qr.h"
#include "ui/touch.h"
#include "web/web_server.h"

#ifndef FW_VERSION
#define FW_VERSION "0.3.0-phase3"
#endif

namespace {
auto& tft = display::tft;
constexpr int16_t ValueX = 74, RowY = 40, RowH = 20;
constexpr int16_t ButtonX = 8, ButtonY = 192, ButtonW = 150, ButtonH = 40;
bool apShown = false;

void header() {
  tft.fillRect(0, 0, SCREEN_W, 26, TFT_NAVY);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("CYD-TESTER", 6, 13, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(FW_VERSION, SCREEN_W - 6, 13, 2);
}

void drawLayout(bool ap) {
  tft.fillScreen(TFT_BLACK);
  header();
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  const char* labels[] = {"Wi-Fi", "SSID", "IP", "RSSI", "Host", "Devices"};
  for (int i = 0; i < 6; ++i) tft.drawString(labels[i], 8, RowY + i * RowH, 2);
  tft.drawFastHLine(0, 151, SCREEN_W, TFT_DARKGREY);
  if (ap) {
    const String& name = wifimgr::unitName();
    const String& pw = unitcfg::current().apPassword;
    const String payload = pw.length() ? "WIFI:T:WPA;S:" + name + ";P:" + pw + ";;" : "WIFI:T:nopass;S:" + name + ";;";
    qr::draw(tft, payload.c_str(), 214, 30, 100);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(name, 264, 136, 2);
  } else {
    tft.fillRoundRect(ButtonX, ButtonY, ButtonW, ButtonH, 6, TFT_DARKCYAN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    tft.drawString("Setup AP", ButtonX + ButtonW / 2, ButtonY + ButtonH / 2, 2);
  }
  apShown = ap;
}

void drawValue(int row, const String& value, uint16_t color = TFT_WHITE) {
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextPadding(apShown ? 130 : 240);
  tft.drawString(value, ValueX, RowY + row * RowH, 2);
  tft.setTextPadding(0);
}

void drawLine(int16_t y, const String& value, uint16_t color) {
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextPadding(SCREEN_W - 16);
  tft.drawString(value, 8, y, 2);
  tft.setTextPadding(0);
}

void refresh() {
  const auto st = wifimgr::state();
  const bool connected = st == wifimgr::State::Connected;
  const uint16_t stateColor = connected ? TFT_GREEN : st == wifimgr::State::Connecting ? TFT_YELLOW : TFT_ORANGE;
  String state = wifimgr::stateText();
  if (!connected && wifimgr::lastFailure().length()) state += ": " + wifimgr::lastFailure();
  drawValue(0, state, stateColor);
  drawValue(1, wifimgr::savedWifi().ssid);
  drawValue(2, connected ? wifimgr::stationIp().toString() : String("--"));
  drawValue(3, connected ? String(wifimgr::rssi()) + " dBm" : String("--"));
  drawValue(4, wifimgr::unitName() + ".local");
  const auto& devs = profiles::devices();
  size_t ok = 0;
  for (const auto& d : devs) ok += d.status == profile::Status::Ok;
  String devices = String(ok) + " of " + String(devs.size()) + " loaded";
  if (!devs.empty() && devs[0].status == profile::Status::Ok)
    devices += ", unit " + String(devs[0].unitId) + " " + devs[0].profile.model.c_str();
  drawValue(5, devices, ok == devs.size() ? TFT_WHITE : TFT_ORANGE);

  if (button::held()) {
    const uint32_t s = button::heldMs() / 1000;
    drawLine(160, s >= 10 ? "Release: network settings reset" : s >= 3 ? "Setup AP on. Keep holding to " + String(10 - s) + " s for network reset"
                                                                : "Setup AP in " + String(3 - s) + " s, network reset at 10 s",
             s >= 10 ? TFT_RED : TFT_YELLOW);
  } else if (apShown) {
    const uint32_t off = wifimgr::apOffInS();
    drawLine(160, "Join " + wifimgr::unitName() + ", open 192.168.4.1", TFT_CYAN);
    drawLine(182, String(wifimgr::apClients()) + " phone(s) connected" + (off ? ", AP closes in " + String(off) + " s" : String()),
             TFT_LIGHTGREY);
  } else {
    drawLine(160, connected ? "Web: http://" + wifimgr::stationIp().toString() + "/" : String("Hold BOOT 3 s for the setup AP"),
             TFT_LIGHTGREY);
  }
  drawLine(SCREEN_H - 10, "Heap " + String(ESP.getFreeHeap() / 1024) + " KB (min " + String(ESP.getMinFreeHeap() / 1024) + ")",
           TFT_DARKGREY);
}

void ledPattern() {
  if (wifimgr::apActive()) led::setPattern(led::Pattern::BlueSolid);
  else if (wifimgr::state() == wifimgr::State::Connected) led::setPattern(led::Pattern::Off);
  else led::setPattern(led::Pattern::YellowSlowBlink);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.printf("\nCYD-TESTER %s\n", FW_VERSION);
  led::begin();
  button::begin();

  const bool fsOk = LittleFS.begin(true);  // the data partition is ours alone
  Serial.printf("LittleFS %s total=%u used=%u\n", fsOk ? "mounted" : "FAILED", LittleFS.totalBytes(), LittleFS.usedBytes());
  String error;
  std::vector<String> warnings;
  if (!unitcfg::load(error, warnings)) Serial.printf("unit.toml: %s (using defaults)\n", error.c_str());
  for (const auto& w : warnings) Serial.printf("unit.toml: %s\n", w.c_str());
  profiles::installBuiltins();
  profiles::loadDevices();

  const auto& dc = unitcfg::current().display;
  display::begin(dc.rotation, dc.brightness);
  touch::begin();
  delay(100);
  const bool forceCal = touch::touchedNow();  // screen held during power-up
  if (forceCal || !touch::loadCalibration()) touch::runCalibration();

  wifimgr::begin();
  web::begin();
  drawLayout(wifimgr::apActive());
}

void loop() {
  static uint32_t lastDraw = 0, lastLog = 0;
  static bool wasPressed = false;
  button::service();
  wifimgr::service();
  ledPattern();
  led::service();

  if (button::crossed(3000)) { Serial.println("BOOT 3 s: setup AP requested"); wifimgr::requestPortal(); }
  if (button::crossed(10000)) { Serial.println("BOOT 10 s: network reset"); wifimgr::resetNetwork(); }

  const touch::Point p = touch::read();
  if (p.pressed && !wasPressed && !apShown && p.x >= ButtonX && p.x < ButtonX + ButtonW && p.y >= ButtonY &&
      p.y < ButtonY + ButtonH) {
    led::flash(led::Color::Blue, 60);
    wifimgr::requestPortal();
  }
  wasPressed = p.pressed;

  if (wifimgr::apActive() != apShown) drawLayout(wifimgr::apActive());
  const uint32_t now = millis();
  if (now - lastDraw >= 250) { lastDraw = now; refresh(); }
  if (now - lastLog >= 10000) {
    lastLog = now;
    Serial.printf("wifi=%s ip=%s ap=%d heap=%u min=%u\n", wifimgr::stateText(), wifimgr::stationIp().toString().c_str(),
                  wifimgr::apActive(), ESP.getFreeHeap(), ESP.getMinFreeHeap());
  }
  delay(5);
}
