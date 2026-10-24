// Drawing: everything that ends up on screen. Ported from reference/demo.html.
#pragma once
#include <M5Cardputer.h>
#include "game.h"

// Draws a whole frame of the game into the off-screen canvas.
// t is the time in ms (drives animation). lost = show "partner lost" on the search screen.
void drawGame(M5Canvas& c, const Game& g, uint32_t t, const char* myName, bool lost);

// Sends the finished frame to the display (shifted sideways while the screen shakes).
void present(M5Canvas& c, uint32_t t);

// Effects: the game starts them, drawGame animates them.
namespace Fx {
void boom(int x, int y, View layer, uint32_t t);    // explosion on a hit
void splash(int x, int y, View layer, uint32_t t);  // splash on a miss
void banner(const char* text, uint32_t col, uint32_t t);  // e.g. "SUNK!", from time t
void shake(uint32_t t);
void aim(int x, int y, uint32_t t);  // target ring while our shot is on its way
void clearAim();
void clear();  // new game
}  // namespace Fx
