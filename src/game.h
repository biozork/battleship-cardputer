// Game state and rules: the fleet, placing ships, shots, and which phase we are in.
#pragma once
#include <Arduino.h>

const int GRID_N = 10;  // the board is 10x10
const int NUM_SHIPS = 5;

struct ShipType {
  const char* name;
  uint8_t size;
};
extern const ShipType SHIP_TYPES[NUM_SHIPS];

struct Ship {
  int8_t x, y;  // the top-left square
  uint8_t size;
  bool horiz;
  uint8_t hits;
  int cellX(int i) const { return horiz ? x + i : x; }
  int cellY(int i) const { return horiz ? y : y + i; }
  bool sunk() const { return hits >= size; }
};

// Ships are placed in SHIP_TYPES order, so ships[i] is always SHIP_TYPES[i].
struct Fleet {
  Ship ships[NUM_SHIPS];
  int count = 0;  // how many are placed so far
  int shipAt(int x, int y) const;  // index of the ship on that square, or -1
  bool fits(const Ship& s) const;  // inside the board and not overlapping
  void add(const Ship& s);
  void autoPlace();  // place the remaining ships at random
  int afloat() const;  // ships not sunk yet
};

// Search: looking for a partner. Place: placing ships. Waiting: our fleet is
// placed, partner's isn't. Battle: taking turns. Over: someone won.
enum class Phase { Search, Place, Waiting, Battle, Over };
enum class View { Shots, Fleet };  // "your shots" (partner's board) or "your fleet"

// What a square looks like on the board.
enum Mark : uint8_t { MARK_NONE, MARK_MISS, MARK_HIT, MARK_SUNK };

// Radio message payloads.
struct __attribute__((packed)) FireMsg {
  uint8_t x, y;
};
struct __attribute__((packed)) ResultMsg {
  uint8_t x, y, hit;
  uint8_t sunk;  // index of the ship that sank, or NO_SHIP
  int8_t sx, sy;  // the sunk ship's position and shape, so the shooter can draw it
  uint8_t size, horiz;
  uint8_t win;  // that was the last ship
};
const uint8_t NO_SHIP = 0xFF;

struct Game {
  Phase phase = Phase::Search;
  View view = View::Fleet;
  Fleet fleet;
  int cursorX = 4, cursorY = 4;
  bool horiz = true;  // rotation of the ship being placed
  bool ready = false, peerReady = false;
  bool myTurn = false, pending = false;  // pending = we fired and wait for the result
  uint8_t shots[GRID_N][GRID_N];     // our shots at the partner, [y][x]
  uint8_t incoming[GRID_N][GRID_N];  // the partner's shots at us, [y][x]
  Ship enemySunk[NUM_SHIPS];         // partner ships we sank, for drawing
  int enemySunkCount = 0;
  bool won = false, wantRematch = false, peerRematch = false;
  uint16_t wins = 0, losses = 0;  // kept across games
  char msg[2][24] = {};  // two-line message in the side panel

  void newGame();  // everything except the score
  void setMsg(const char* line1, const char* line2 = "");
  Ship ghost() const;  // the next ship to place, at the cursor
  ResultMsg takeFire(int x, int y);       // the partner fired at us
  void applyResult(const ResultMsg& r);  // the answer to our shot
};

// "B7" style name of a square.
void coordName(int x, int y, char* out);
