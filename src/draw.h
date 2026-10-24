// Drawing: everything that ends up on screen. Ported from reference/demo.html.
#pragma once
#include <M5Cardputer.h>
#include "game.h"

// Draws a whole frame of the game into the off-screen canvas.
// t is the time in ms (drives animation). lost = show "partner lost" on the search screen.
void drawGame(M5Canvas& c, const Game& g, uint32_t t, const char* myName, bool lost);
