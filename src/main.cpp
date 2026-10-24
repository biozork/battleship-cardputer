// Cardputer Battleship: two players, two devices, ESP-NOW radio.
// This file is the game's "director": it reacts to keys and radio messages
// and decides what happens next. Drawing lives in draw.cpp, rules in game.cpp,
// the radio in link.cpp.

#include <M5Cardputer.h>
#include <Preferences.h>
#include "draw.h"
#include "game.h"
#include "input.h"
#include "link.h"
#include "sound.h"

static M5Canvas canvas(&M5Cardputer.Display);  // the off-screen buffer, 240x135
static Game game;
static Preferences prefs;  // NVS: small settings storage that survives power off

static const uint32_t FRAME_MS = 33;         // about 30 frames per second
static const uint32_t SHOW_IMPACT_MS = 1300;  // how long we look at our fleet after being shot
static uint32_t lostUntil = 0;   // show "partner lost" on the search screen until then
static uint32_t backToShotsAt = 0;  // when to switch back to "your shots" after an impact

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = also start the keyboard
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Speaker.setVolume(128);

  canvas.setColorDepth(16);
  canvas.setPsram(false);  // the ADV has no PSRAM; the buffer (~65 KB) fits in normal RAM
  if (!canvas.createSprite(240, 135)) Serial.println("Not enough memory for the screen buffer!");

  prefs.begin("battleship");
  game.wins = prefs.getUShort("wins", 0);
  game.losses = prefs.getUShort("losses", 0);

  Link::begin();
  Serial.printf("I am %s, score %u-%u\n", Link::myName(), game.wins, game.losses);
}

static void saveScore() {
  prefs.putUShort("wins", game.wins);
  prefs.putUShort("losses", game.losses);
}

static void startPlacement() {
  game.newGame();
  game.phase = Phase::Place;
  backToShotsAt = 0;
  Fx::clear();
}

// ---------- ready / who goes first ----------
static void startBattle(bool first) {
  game.phase = Phase::Battle;
  game.myTurn = first;
  game.view = first ? View::Shots : View::Fleet;
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

// ---------- battle ----------
static void fire() {
  char sq[4];
  coordName(game.cursorX, game.cursorY, sq);
  if (!game.myTurn || game.pending) {
    game.setMsg("Not your turn.");
    Sound::bad();
    return;
  }
  if (game.shots[game.cursorY][game.cursorX] != MARK_NONE) {
    char line[24];
    snprintf(line, sizeof line, "at %s.", sq);
    game.setMsg("Already fired", line);
    Sound::bad();
    return;
  }
  FireMsg f = {(uint8_t)game.cursorX, (uint8_t)game.cursorY};
  if (!Link::send(MSG_FIRE, &f, sizeof f)) return;
  game.pending = true;
  Fx::aim(game.cursorX, game.cursorY, millis());
  char line[24];
  snprintf(line, sizeof line, "Firing at %s...", sq);
  game.setMsg(line);
  Sound::fire();
}

// The partner fired at us.
static void onFire(const Message& m, uint32_t now) {
  if (game.phase != Phase::Battle || game.myTurn || m.len < sizeof(FireMsg)) return;
  FireMsg f;
  memcpy(&f, m.data, sizeof f);
  if (f.x >= GRID_N || f.y >= GRID_N) return;

  ResultMsg r = game.takeFire(f.x, f.y);
  Link::send(MSG_RESULT, &r, sizeof r);

  char line[24], sq[4];
  coordName(f.x, f.y, sq);
  snprintf(line, sizeof line, "They fired %s:", sq);
  if (r.sunk != NO_SHIP) {
    char lost[24];
    snprintf(lost, sizeof lost, "lost: %s", SHIP_TYPES[r.sunk].name);
    game.setMsg(line, lost);
  } else {
    game.setMsg(line, r.hit ? "hit!" : "miss.");
  }
  game.view = View::Fleet;  // look at where it landed
  if (r.hit) {
    Fx::boom(f.x, f.y, View::Fleet, now);
    Fx::shake(now);
  } else {
    Fx::splash(f.x, f.y, View::Fleet, now);
  }
  if (r.sunk != NO_SHIP) Fx::banner("SUNK!", 0xFF8787, now + 350);

  if (r.win) {
    game.phase = Phase::Over;
    game.won = false;
    game.losses++;
    saveScore();
    Sound::lose();
    return;
  }
  if (r.sunk != NO_SHIP) Sound::sunk();
  else if (r.hit) Sound::hit();
  else Sound::miss();
  game.myTurn = true;
  backToShotsAt = now + SHOW_IMPACT_MS;
}

// The answer to our shot.
static void onResult(const Message& m) {
  if (game.phase != Phase::Battle || !game.pending || m.len < sizeof(ResultMsg)) return;
  ResultMsg r;
  memcpy(&r, m.data, sizeof r);
  if (r.x >= GRID_N || r.y >= GRID_N) return;

  game.applyResult(r);
  uint32_t now = millis();
  Fx::clearAim();
  if (r.hit) {
    Fx::boom(r.x, r.y, View::Shots, now);
    Fx::shake(now);
  } else {
    Fx::splash(r.x, r.y, View::Shots, now);
  }
  if (r.sunk != NO_SHIP && !r.win) Fx::banner("SUNK!", 0xFFD43B, now + 350);
  game.pending = false;
  game.myTurn = false;
  char sq[8];
  coordName(r.x, r.y, sq);
  strlcat(sq, ":", sizeof sq);
  if (r.sunk != NO_SHIP && r.sunk < NUM_SHIPS) {
    char line[24];
    snprintf(line, sizeof line, "sunk %s!", SHIP_TYPES[r.sunk].name);
    game.setMsg(sq, line);
  } else {
    game.setMsg(sq, r.hit ? "HIT!" : "miss.");
  }

  if (r.win) {
    game.phase = Phase::Over;
    game.won = true;
    game.wins++;
    saveScore();
    Sound::win();
  } else if (r.sunk != NO_SHIP) {
    Sound::sunk();
  } else if (r.hit) {
    Sound::hit();
  } else {
    Sound::miss();
  }
}

// ---------- rematch ----------
static void requestRematch() {
  if (game.wantRematch) return;
  game.wantRematch = true;
  Link::send(MSG_REMATCH);
  if (game.peerRematch) {
    startPlacement();
    game.setMsg("New game!");
  }
}

static void onRematch() {
  game.peerRematch = true;
  if (game.phase == Phase::Over && game.wantRematch) {
    startPlacement();
    game.setMsg("New game!");
  }
}

// ---------- keys ----------
static void moveCursor(const KeyEvent& k) {
  switch (k.key) {
    case Key::Up:    if (game.cursorY > 0) game.cursorY--; break;
    case Key::Down:  if (game.cursorY < GRID_N - 1) game.cursorY++; break;
    case Key::Left:  if (game.cursorX > 0) game.cursorX--; break;
    case Key::Right: if (game.cursorX < GRID_N - 1) game.cursorX++; break;
    default: break;
  }
}

static void handlePlaceKey(const KeyEvent& k) {
  moveCursor(k);
  if (k.key == Key::Ok) {
    Ship s = game.ghost();
    if (game.fleet.fits(s)) {
      game.fleet.add(s);
      Sound::place();
      if (game.fleet.count == NUM_SHIPS) becomeReady();
    } else {
      Sound::bad();
    }
  } else if (k.key == Key::Char && k.ch == 'r') {
    game.horiz = !game.horiz;
  } else if (k.key == Key::Char && k.ch == 'a') {
    game.fleet.autoPlace();
    Sound::place();
    becomeReady();
  }
}

static void handleBattleKey(const KeyEvent& k) {
  moveCursor(k);
  if (k.key == Key::Tab) {
    game.view = game.view == View::Shots ? View::Fleet : View::Shots;
    backToShotsAt = 0;
  } else if (k.key == Key::Ok) {
    if (game.view != View::Shots) {  // first ok just brings up the shots view
      game.view = View::Shots;
      backToShotsAt = 0;
    } else {
      fire();
    }
  }
}

static void handleKey(const KeyEvent& k) {
  if (k.key == Key::Char && k.ch == 'm') {
    Sound::toggle();
    return;
  }
  switch (game.phase) {
    case Phase::Place:  handlePlaceKey(k); break;
    case Phase::Battle: handleBattleKey(k); break;
    case Phase::Over:
      if (k.key == Key::Ok) requestRematch();
      if (k.key == Key::Tab) game.view = game.view == View::Shots ? View::Fleet : View::Shots;
      break;
    default: break;
  }
}

// ---------- main loop ----------
void loop() {
  M5Cardputer.update();
  uint32_t now = millis();

  LinkEvent ev = Link::update();
  if (ev == LinkEvent::Connected) {
    Serial.printf("Linked with %s\n", Link::peerName());
    startPlacement();
    char found[24];
    snprintf(found, sizeof found, "Found %s!", Link::peerName() + 10);  // just the 4 hex digits
    game.setMsg(found);
    Sound::link();
  } else if (ev == LinkEvent::Lost) {
    Serial.println("Partner lost");
    game.newGame();
    game.phase = Phase::Search;
    Fx::clear();
    lostUntil = now + 5000;
    Sound::lost();
  }

  Message m;
  while (Link::receive(m)) {
    switch (m.type) {
      case MSG_READY:   onReady(m); break;
      case MSG_FIRE:    onFire(m, now); break;
      case MSG_RESULT:  onResult(m); break;
      case MSG_REMATCH: onRematch(); break;
    }
  }

  // After showing the impact on our fleet, go back to aiming.
  if (backToShotsAt && now >= backToShotsAt) {
    backToShotsAt = 0;
    if (game.phase == Phase::Battle && game.myTurn) {
      game.view = View::Shots;
      Sound::turn();
    }
  }

  KeyEvent k = pollKey();
  if (k.key != Key::None) handleKey(k);

  static uint32_t lastFrame = 0;
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    drawGame(canvas, game, now, Link::myName(), now < lostUntil);
    present(canvas, now);
  }
}
