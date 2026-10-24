// Drawing: everything that ends up on screen. Ported from reference/demo.html.
#pragma once
#include <M5Cardputer.h>

const int GRID_N = 10;  // the board is 10x10

// Everything the drawing code needs to know to draw one frame.
struct Screen {
  int cursorX, cursorY;  // 0..9
  bool fog;              // true = dark "your shots" sea, false = "your fleet" sea
  int fps;               // measured frames per second, shown for testing
};

// Draws a whole frame into the off-screen canvas. t is the time in ms (drives animation).
void drawFrame(M5Canvas& c, const Screen& s, uint32_t t);

// Radar screen while looking for a partner. lost = show "partner lost".
void drawSearch(M5Canvas& c, uint32_t t, const char* myName, bool lost);

// Milestone 3 test screen: link details and counters.
struct LinkTestInfo {
  const char* myName;
  const char* peerName;
  uint32_t heartbeats, pingsSent, pingsAcked, pingsIn, resends, rttMs;
  bool flash;  // a ping just arrived
};
void drawLinkTest(M5Canvas& c, const LinkTestInfo& s, uint32_t t);
