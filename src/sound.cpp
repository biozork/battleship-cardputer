#include <M5Cardputer.h>
#include "sound.h"

static bool enabled = false;  // opt-in: always silent after power on, m turns it on

// Plays notes one after another: {frequency Hz, length ms}.
static void play(const uint16_t notes[][2], int n) {
  if (!enabled) return;
  // channel 0; the first note interrupts whatever is playing, the rest queue behind it
  for (int i = 0; i < n; i++) M5Cardputer.Speaker.tone(notes[i][0], notes[i][1], 0, i == 0);
}

namespace Sound {

void toggle() {
  enabled = !enabled;
  if (enabled) {
    const uint16_t n[][2] = {{880, 40}};
    play(n, 1);
  }
}

bool on() { return enabled; }

void place() {
  const uint16_t n[][2] = {{300, 30}, {520, 40}};
  play(n, 2);
}

void bad() {
  const uint16_t n[][2] = {{170, 70}, {130, 80}};
  play(n, 2);
}

void link() {
  const uint16_t n[][2] = {{900, 90}, {1350, 120}};
  play(n, 2);
}

void lost() {
  const uint16_t n[][2] = {{400, 150}, {250, 200}};
  play(n, 2);
}

void turn() {
  const uint16_t n[][2] = {{660, 70}, {880, 100}};
  play(n, 2);
}

void fire() {
  const uint16_t n[][2] = {{1400, 40}, {900, 50}, {500, 70}};
  play(n, 3);
}

void hit() {  // low rumble
  const uint16_t n[][2] = {{110, 120}, {80, 150}, {55, 220}};
  play(n, 3);
}

void miss() {  // falling "splash"
  const uint16_t n[][2] = {{520, 60}, {350, 80}, {200, 110}};
  play(n, 3);
}

void sunk() {  // rumble, then a short descending tune
  const uint16_t n[][2] = {{110, 120}, {70, 200}, {392, 150}, {330, 150}, {262, 150}, {196, 250}};
  play(n, 6);
}

void win() {
  const uint16_t n[][2] = {{523, 160}, {659, 160}, {784, 160}, {1047, 350}};
  play(n, 4);
}

void lose() {
  const uint16_t n[][2] = {{392, 220}, {311, 220}, {262, 450}};
  play(n, 3);
}

}  // namespace Sound
