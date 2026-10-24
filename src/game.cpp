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

int Fleet::afloat() const {
  int n = 0;
  for (int i = 0; i < count; i++)
    if (!ships[i].sunk()) n++;
  return n;
}

void Game::newGame() {
  fleet.count = 0;
  view = View::Fleet;
  cursorX = cursorY = 4;
  horiz = true;
  ready = peerReady = myTurn = pending = false;
  memset(shots, MARK_NONE, sizeof shots);
  memset(incoming, MARK_NONE, sizeof incoming);
  enemySunkCount = 0;
  won = wantRematch = peerRematch = false;
  setMsg("");
}

void Game::setMsg(const char* line1, const char* line2) {
  strlcpy(msg[0], line1, sizeof msg[0]);
  strlcpy(msg[1], line2, sizeof msg[1]);
}

Ship Game::ghost() const {
  return {(int8_t)cursorX, (int8_t)cursorY, SHIP_TYPES[fleet.count < NUM_SHIPS ? fleet.count : 0].size, horiz, 0};
}

ResultMsg Game::takeFire(int x, int y) {
  ResultMsg r = {(uint8_t)x, (uint8_t)y, 0, NO_SHIP, 0, 0, 0, 0, 0};
  int s = fleet.shipAt(x, y);
  if (s < 0) {
    incoming[y][x] = MARK_MISS;
    return r;
  }
  if (incoming[y][x] != MARK_HIT) fleet.ships[s].hits++;  // the same square never counts twice
  incoming[y][x] = MARK_HIT;
  r.hit = 1;
  const Ship& ship = fleet.ships[s];
  if (ship.sunk()) {
    r.sunk = s;
    r.sx = ship.x;
    r.sy = ship.y;
    r.size = ship.size;
    r.horiz = ship.horiz;
    r.win = fleet.afloat() == 0;
  }
  return r;
}

void Game::applyResult(const ResultMsg& r) {
  shots[r.y][r.x] = r.hit ? MARK_HIT : MARK_MISS;
  if (r.sunk == NO_SHIP || enemySunkCount >= NUM_SHIPS) return;
  Ship s = {r.sx, r.sy, r.size, (bool)r.horiz, r.size};
  for (int i = 0; i < s.size; i++) {
    int x = s.cellX(i), y = s.cellY(i);
    if (x >= 0 && y >= 0 && x < GRID_N && y < GRID_N) shots[y][x] = MARK_SUNK;
  }
  enemySunk[enemySunkCount++] = s;
}

void coordName(int x, int y, char* out) {
  out[0] = "ABCDEFGHIJ"[x];
  snprintf(out + 1, 3, "%d", y + 1);
}
