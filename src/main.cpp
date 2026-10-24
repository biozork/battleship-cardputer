// Milestone 3: ESP-NOW link test.
// The two Cardputers find each other over the radio and show live counters.
// Press ok to send a "ping": the other device beeps and counts it.

#include <M5Cardputer.h>
#include "draw.h"
#include "input.h"
#include "link.h"

static M5Canvas canvas(&M5Cardputer.Display);  // the off-screen buffer, 240x135

static const uint32_t FRAME_MS = 33;  // about 30 frames per second

static uint32_t pingsIn = 0;
static uint32_t flashUntil = 0;  // highlight the screen briefly when a ping arrives
static uint32_t lostUntil = 0;   // show "partner lost" on the search screen until then

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = also start the keyboard
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Speaker.setVolume(128);

  canvas.setColorDepth(16);
  canvas.setPsram(false);  // the ADV has no PSRAM; the buffer (~65 KB) fits in normal RAM
  if (!canvas.createSprite(240, 135)) Serial.println("Not enough memory for the screen buffer!");

  Link::begin();
  Serial.printf("I am %s\n", Link::myName());
}

void loop() {
  M5Cardputer.update();
  uint32_t now = millis();

  LinkEvent ev = Link::update();
  if (ev == LinkEvent::Connected) {
    Serial.printf("Linked with %s\n", Link::peerName());
    pingsIn = 0;
    M5Cardputer.Speaker.tone(900, 90);  // "link found": two rising notes
    M5Cardputer.Speaker.tone(1350, 120, -1, false);
  } else if (ev == LinkEvent::Lost) {
    Serial.println("Partner lost");
    lostUntil = now + 5000;
    M5Cardputer.Speaker.tone(400, 150);
    M5Cardputer.Speaker.tone(250, 200, -1, false);
  }

  Message m;
  while (Link::receive(m)) {
    if (m.type == MSG_PING) {
      pingsIn++;
      flashUntil = now + 200;
      M5Cardputer.Speaker.tone(1500, 50);
    }
  }

  KeyEvent k = pollKey();
  if (k.key == Key::Ok && Link::connected()) {
    if (Link::send(MSG_PING)) M5Cardputer.Speaker.tone(700, 30);
  }

  static uint32_t lastFrame = 0;
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    if (Link::connected()) {
      const LinkStats& s = Link::stats();
      LinkTestInfo info = {Link::myName(), Link::peerName(), s.heartbeatsIn, s.sent, s.acked,
                           pingsIn, s.resends, s.lastRttMs, now < flashUntil};
      drawLinkTest(canvas, info, now);
    } else {
      drawSearch(canvas, now, Link::myName(), now < lostUntil);
    }
    canvas.pushSprite(0, 0);
  }
}
