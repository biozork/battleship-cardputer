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
