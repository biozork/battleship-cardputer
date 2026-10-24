// Game state and rules: the fleet, placing ships, and which phase we are in.
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

struct Fleet {
  Ship ships[NUM_SHIPS];
  int count = 0;  // how many are placed so far
  int shipAt(int x, int y) const;  // index of the ship on that square, or -1
  bool fits(const Ship& s) const;  // inside the board and not overlapping
  void add(const Ship& s);
  void autoPlace();  // place the remaining ships at random
};

// Search: looking for a partner. Place: placing ships. Waiting: our fleet is
// placed, partner's isn't. Battle: both ready (the shooting comes in milestone 5).
enum class Phase { Search, Place, Waiting, Battle };

struct Game {
  Phase phase = Phase::Search;
  Fleet fleet;
  int cursorX = 4, cursorY = 4;
  bool horiz = true;  // rotation of the ship being placed
  bool ready = false, peerReady = false, myTurn = false;
  char msg[2][24] = {};  // two-line message in the side panel

  void newGame();
  void setMsg(const char* line1, const char* line2 = "");
  Ship ghost() const;  // the next ship to place, at the cursor
};
