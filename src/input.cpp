#include <M5Cardputer.h>
#include "input.h"

static const uint32_t REPEAT_DELAY = 350;  // ms an arrow must be held before it repeats
static const uint32_t REPEAT_EVERY = 90;   // ms between repeats after that

static KeyEvent heldKey = {Key::None, 0};
static uint32_t nextRepeat = 0;

// What is pressed right now. Arrow keys report ; . , / on the Cardputer.
static KeyEvent readKeys() {
  auto ks = M5Cardputer.Keyboard.keysState();
  if (ks.enter) return {Key::Ok, 0};
  if (ks.del) return {Key::Del, 0};
  if (ks.tab) return {Key::Tab, 0};
  if (ks.word.empty()) return {Key::None, 0};
  char c = ks.word[0];
  switch (c) {
    case ';': return {Key::Up, 0};
    case '.': return {Key::Down, 0};
    case ',': return {Key::Left, 0};
    case '/': return {Key::Right, 0};
    case '`': return {Key::Esc, 0};
  }
  return {Key::Char, (char)tolower(c)};
}

static bool isArrow(Key k) {
  return k == Key::Up || k == Key::Down || k == Key::Left || k == Key::Right;
}

KeyEvent pollKey() {
  if (!M5Cardputer.Keyboard.isPressed()) {
    heldKey = {Key::None, 0};
    return heldKey;
  }
  KeyEvent now = readKeys();
  uint32_t ms = millis();
  if (now.key != heldKey.key || now.ch != heldKey.ch) {  // a new key went down
    heldKey = now;
    nextRepeat = ms + REPEAT_DELAY;
    return now;
  }
  if (isArrow(now.key) && ms >= nextRepeat) {  // still held: auto-repeat
    nextRepeat = ms + REPEAT_EVERY;
    return now;
  }
  return {Key::None, 0};
}
