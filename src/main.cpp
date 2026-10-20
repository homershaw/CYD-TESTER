// RIOT Modbus TCP Test Unit (CYD-TESTER)
// Phase 1: board bring-up - display, touch (with calibration), RGB LED, BOOT button, LDR.
#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "ui/button.h"
#include "ui/display.h"
#include "ui/led.h"
#include "ui/touch.h"

#ifndef FW_VERSION
#define FW_VERSION "0.1.0-phase1"
#endif

namespace {
String unitName;  // riot-XXXX, from the last 4 hex digits of the station MAC

String makeUnitName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char name[12];
  snprintf(name, sizeof name, "riot-%02X%02X", mac[4], mac[5]);
  return name;
}

void drawStatic() {
  auto& tft = display::tft;
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, SCREEN_W, 26, TFT_NAVY);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("CYD-TESTER  bring-up", 6, 13, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(FW_VERSION, SCREEN_W - 6, 13, 2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  const char* labels[] = {"Unit", "Light (LDR)", "BOOT button", "Touch", "Free heap"};
  for (int i = 0; i < 5; ++i) tft.drawString(labels[i], 8, 44 + i * 22, 2);
  tft.drawFastHLine(0, 156, SCREEN_W, TFT_DARKGREY);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Draw here to test touch", 8, 168, 2);
}

void drawValue(int row, const String& value, uint16_t color = TFT_WHITE) {
  auto& tft = display::tft;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextPadding(200);
  tft.drawString(value, 116, 44 + row * 22, 2);
  tft.setTextPadding(0);
}

void ledSelfTest() {
  for (auto c : {led::Color::Red, led::Color::Green, led::Color::Blue}) {
    led::flash(c, 250);
    const uint32_t until = millis() + 300;
    while (int32_t(until - millis()) > 0) { led::service(); delay(10); }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  WiFi.mode(WIFI_STA);  // needed for the station MAC; no connection yet in phase 1
  unitName = makeUnitName();
  Serial.printf("\nCYD-TESTER %s  unit=%s\n", FW_VERSION, unitName.c_str());

  led::begin();
  button::begin();
  pinMode(PIN_LDR, INPUT);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);

  // The data partition is ours alone, so formatting an unformatted one on first boot is safe.
  const bool fsOk = LittleFS.begin(true);
  Serial.printf("LittleFS %s total=%u used=%u\n", fsOk ? "mounted" : "FAILED",
                LittleFS.totalBytes(), LittleFS.usedBytes());

  display::begin(1, 80);
  touch::begin();
  delay(100);
  const bool forceCal = touch::touchedNow();  // screen held during power-up
  if (forceCal || !touch::loadCalibration()) {
    Serial.println(forceCal ? "Touch held at power-up: recalibrating" : "No touch calibration: calibrating");
    touch::runCalibration();
  } else {
    const auto& c = touch::calibration();
    Serial.printf("Touch calibration loaded: swap=%d x=%d..%d y=%d..%d\n", c.swapXY, c.xRawAt0, c.xRawAtMax,
                  c.yRawAt0, c.yRawAtMax);
  }

  drawStatic();
  drawValue(0, unitName + (fsOk ? "   FS ok" : "   FS FAILED"), fsOk ? TFT_WHITE : TFT_RED);
  ledSelfTest();
  led::setPattern(led::Pattern::YellowSlowBlink);  // no Wi-Fi yet
}

void loop() {
  static uint32_t lastDraw = 0, lastLog = 0;
  static touch::Point last;
  button::service();
  led::service();

  const touch::Point p = touch::read();
  if (p.pressed) {
    display::tft.fillCircle(p.x, p.y, 2, TFT_CYAN);
    if (!last.pressed) led::flash(led::Color::Green, 30);
  }
  last = p;

  if (button::crossed(3000)) { led::flash(led::Color::Blue, 400); Serial.println("BOOT held 3 s (setup AP in phase 2)"); }
  if (button::crossed(10000)) { led::flash(led::Color::Red, 600); Serial.println("BOOT held 10 s (network reset in phase 2)"); }

  const uint32_t now = millis();
  if (now - lastDraw >= 200) {
    lastDraw = now;
#if HAS_LDR
    drawValue(1, String(analogRead(PIN_LDR)) + " / 4095");
#else
    drawValue(1, "not fitted on this board", TFT_DARKGREY);
#endif
    if (button::held()) {
      const uint32_t s = button::heldMs() / 1000;
      const String hint = s >= 10 ? "release: network reset" : s >= 3 ? "setup AP (reset at 10 s)" : "AP in " + String(3 - s) + " s";
      drawValue(2, "HELD " + String(s) + " s  " + hint, s >= 10 ? TFT_RED : s >= 3 ? TFT_YELLOW : TFT_WHITE);
    } else {
      drawValue(2, "released");
    }
    drawValue(3, p.pressed ? "x=" + String(p.x) + "  y=" + String(p.y) : String("--"));
    drawValue(4, String(ESP.getFreeHeap()) + " B  (min " + String(ESP.getMinFreeHeap()) + ")");
  }
  if (now - lastLog >= 5000) {
    lastLog = now;
    Serial.printf("heap=%u min=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  }
  delay(5);
}
