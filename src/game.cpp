#include "game.h"

const ShipType SHIP_TYPES[NUM_SHIPS] = {
    {"Carrier", 5}, {"Battleship", 4}, {"Cruiser", 3}, {"Submarine", 3}, {"Destroyer", 2},
};

int Fleet::shipAt(int x, int y) const {
  for (int s = 0; s < count; s++)
    for (int i = 0; i < ships[s].size; i++)
      if (ships[s].cellX(i) == x && ships[s].cellY(i) == y) return s;
  return -1;
}

bool Fleet::fits(const Ship& s) const {
  for (int i = 0; i < s.size; i++) {
    int x = s.cellX(i), y = s.cellY(i);
    if (x < 0 || y < 0 || x >= GRID_N || y >= GRID_N || shipAt(x, y) >= 0) return false;
  }
  return true;
}

void Fleet::add(const Ship& s) {
  if (count < NUM_SHIPS) ships[count++] = s;
}

void Fleet::autoPlace() {
  while (count < NUM_SHIPS) {
    Ship s = {0, 0, SHIP_TYPES[count].size, false, 0};
    do {  // random spots until one fits; with an empty-ish board this is quick
      s.horiz = random(2);
      s.x = random(GRID_N);
      s.y = random(GRID_N);
    } while (!fits(s));
    add(s);
  }
}

void Game::newGame() {
  fleet.count = 0;
  cursorX = cursorY = 4;
  horiz = true;
  ready = peerReady = myTurn = false;
  setMsg("");
}

void Game::setMsg(const char* line1, const char* line2) {
  strlcpy(msg[0], line1, sizeof msg[0]);
  strlcpy(msg[1], line2, sizeof msg[1]);
}

Ship Game::ghost() const {
  return {(int8_t)cursorX, (int8_t)cursorY, SHIP_TYPES[fleet.count < NUM_SHIPS ? fleet.count : 0].size, horiz, 0};
}
