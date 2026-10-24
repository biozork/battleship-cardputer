// Milestone 4: placement.
// The devices pair up, each player places five ships, and when both are
// ready the one who finished first gets the first shot.

#include <M5Cardputer.h>
#include "draw.h"
#include "game.h"
#include "input.h"
#include "link.h"
#include "sound.h"

static M5Canvas canvas(&M5Cardputer.Display);  // the off-screen buffer, 240x135
static Game game;

static const uint32_t FRAME_MS = 33;  // about 30 frames per second
static uint32_t lostUntil = 0;        // show "partner lost" on the search screen until then

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

static void startBattle(bool first) {
  game.phase = Phase::Battle;
  game.myTurn = first;
  if (first) {
    game.setMsg("Ready first:", "you fire first!");
    Sound::turn();
  } else {
    game.setMsg("Partner fires", "first. Brace!");
  }
}

// Our fleet is placed. The READY message says whether we had already heard
// the partner's READY, so both sides agree on who was first.
static void becomeReady() {
  game.phase = Phase::Waiting;
  game.ready = true;
  uint8_t sawYours = game.peerReady;
  Link::send(MSG_READY, &sawYours, 1);
  if (game.peerReady) startBattle(false);
}

static void onReady(const Message& m) {
  game.peerReady = true;
  if (game.ready) {
    bool theySawMine = m.len > 0 && m.data[0];
    // If they had heard our READY, we were first. If not, both pressed at
    // the same moment: the MAC addresses decide.
    startBattle(theySawMine || Link::iAmLower());
  } else {
    game.setMsg("Partner is ready.");
  }
}

static void handlePlaceKey(const KeyEvent& k) {
  switch (k.key) {
    case Key::Up:    if (game.cursorY > 0) game.cursorY--; break;
    case Key::Down:  if (game.cursorY < GRID_N - 1) game.cursorY++; break;
    case Key::Left:  if (game.cursorX > 0) game.cursorX--; break;
    case Key::Right: if (game.cursorX < GRID_N - 1) game.cursorX++; break;
    case Key::Ok: {
      Ship s = game.ghost();
      if (game.fleet.fits(s)) {
        game.fleet.add(s);
        Sound::place();
        if (game.fleet.count == NUM_SHIPS) becomeReady();
      } else {
        Sound::bad();
      }
      break;
    }
    case Key::Char:
      if (k.ch == 'r') game.horiz = !game.horiz;
      if (k.ch == 'a') {
        game.fleet.autoPlace();
        Sound::place();
        becomeReady();
      }
      break;
    default: break;
  }
}

void loop() {
  M5Cardputer.update();
  uint32_t now = millis();

  LinkEvent ev = Link::update();
  if (ev == LinkEvent::Connected) {
    Serial.printf("Linked with %s\n", Link::peerName());
    game.newGame();
    game.phase = Phase::Place;
    char found[24];
    snprintf(found, sizeof found, "Found %s!", Link::peerName() + 10);  // just the 4 hex digits
    game.setMsg(found);
    Sound::link();
  } else if (ev == LinkEvent::Lost) {
    Serial.println("Partner lost");
    game.newGame();
    game.phase = Phase::Search;
    lostUntil = now + 5000;
    Sound::lost();
  }

  Message m;
  while (Link::receive(m)) {
    if (m.type == MSG_READY) onReady(m);
  }

  KeyEvent k = pollKey();
  if (k.key == Key::Char && k.ch == 'm') Sound::toggle();
  else if (k.key != Key::None && game.phase == Phase::Place) handlePlaceKey(k);

  static uint32_t lastFrame = 0;
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    drawGame(canvas, game, now, Link::myName(), now < lostUntil);
    canvas.pushSprite(0, 0);
  }
}
