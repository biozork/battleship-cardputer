#include <M5Cardputer.h>
#include "sound.h"

static bool enabled = true;

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

}  // namespace Sound
