// Milestone 2: keyboard and screen.
// Draws the board and lets you move the cursor around with the arrow keys.
// Each frame is drawn into an off-screen canvas, then sent to the display in
// one go, so nothing flickers.

#include <M5Cardputer.h>
#include "draw.h"
#include "input.h"

static M5Canvas canvas(&M5Cardputer.Display);  // the off-screen buffer, 240x135
static Screen screen = {4, 4, false, 0};

static const uint32_t FRAME_MS = 33;  // about 30 frames per second

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // true = also start the keyboard
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Speaker.setVolume(128);

  canvas.setColorDepth(16);
  canvas.setPsram(false);  // the ADV has no PSRAM; the buffer (~65 KB) fits in normal RAM
  if (!canvas.createSprite(240, 135)) Serial.println("Not enough memory for the screen buffer!");
}

static void handleKey(const KeyEvent& k) {
  switch (k.key) {
    case Key::Up:    if (screen.cursorY > 0) screen.cursorY--; break;
    case Key::Down:  if (screen.cursorY < GRID_N - 1) screen.cursorY++; break;
    case Key::Left:  if (screen.cursorX > 0) screen.cursorX--; break;
    case Key::Right: if (screen.cursorX < GRID_N - 1) screen.cursorX++; break;
    case Key::Tab:   screen.fog = !screen.fog; break;
    case Key::Ok:    M5Cardputer.Speaker.tone(520, 70); break;
    default: break;
  }
}

void loop() {
  M5Cardputer.update();
  KeyEvent k = pollKey();
  if (k.key != Key::None) handleKey(k);

  static uint32_t lastFrame = 0, fpsStart = 0, frames = 0;
  uint32_t now = millis();
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    drawFrame(canvas, screen, now);
    canvas.pushSprite(0, 0);
    frames++;
    if (now - fpsStart >= 1000) {
      screen.fps = frames;
      frames = 0;
      fpsStart = now;
    }
  }
}
