// Milestone 1: hello world.
// Checks that the display, keyboard and speaker all work, and shows this
// device's MAC address (the radio "name" we will use for ESP-NOW later).

#include <M5Cardputer.h>
#include <WiFi.h>

static String macText;
static String lastKey = "-";
static uint32_t keyCount = 0;

static void drawScreen() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(TFT_NAVY);

  d.setTextColor(TFT_WHITE, TFT_NAVY);
  d.setTextDatum(top_center);
  d.setFont(&fonts::FreeSansBold12pt7b);
  d.drawString("BATTLESHIP", d.width() / 2, 8);

  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_CYAN, TFT_NAVY);
  d.drawString("hello, Cardputer!", d.width() / 2, 38);

  d.setTextDatum(top_left);
  d.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
  d.drawString("MAC: " + macText, 8, 62);
  d.drawString("Last key: " + lastKey, 8, 82);
  d.drawString("Keys pressed: " + String(keyCount), 8, 102);
}

// Turn the current key state into readable text, e.g. "a", "ok", "up".
static String describeKeys(const Keyboard_Class::KeysState& ks) {
  if (ks.enter) return "ok";
  if (ks.del) return "del";
  if (ks.tab) return "tab";
  if (ks.word.empty()) return "";
  char c = ks.word[0];
  switch (c) {
    case ';': return "up";
    case '.': return "down";
    case ',': return "left";
    case '/': return "right";
    case '`': return "esc";
    case ' ': return "space";
  }
  return String(c);
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = also start the keyboard
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Speaker.setVolume(128);

  WiFi.mode(WIFI_STA);  // the radio must be on to read the MAC address
  macText = WiFi.macAddress();
  Serial.println("Hello from Cardputer, MAC " + macText);

  drawScreen();
  M5Cardputer.Speaker.tone(880, 80);  // startup beep
}

void loop() {
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    String k = describeKeys(M5Cardputer.Keyboard.keysState());
    if (k.length() > 0) {
      lastKey = k;
      keyCount++;
      Serial.println("Key: " + k);
      M5Cardputer.Speaker.tone(1200, 30);  // short click
      drawScreen();
    }
  }
  delay(10);
}
