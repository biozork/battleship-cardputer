#include "draw.h"

// Layout, same numbers as the mock: grid at (GX,GY), 11 px cells, panel at PX.
static const int N = GRID_N, CS = 11, GX = 16, GY = 12, PX = 134;
static const char* COLS = "ABCDEFGHIJ";

// ---------- colours ----------
// Colours are written as 0xRRGGBB like in the mock. The screen uses 16-bit
// colour (RGB565), so they are converted right before drawing.
static uint16_t rgb(uint32_t c) {
  uint8_t r = c >> 16, g = (c >> 8) & 0xFF, b = c & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Blend from colour a towards colour b. k = 0 gives a, k = 1 gives b.
// This stands in for the mock's see-through rgba() colours.
static uint32_t mix(uint32_t a, uint32_t b, float k) {
  if (k < 0) k = 0;
  if (k > 1) k = 1;
  uint32_t out = 0;
  for (int shift = 16; shift >= 0; shift -= 8) {
    int ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
    out |= (uint32_t)(ca + (cb - ca) * k + 0.5f) << shift;
  }
  return out;
}

static const uint32_t C_YELLOW = 0xFFD43B, C_WHITE = 0xE8EEF0, C_MUTED = 0x9FB3BD, C_LABEL = 0x6F8794;

// ---------- small helpers ----------
static void text(M5Canvas& c, const char* s, int x, int y, uint32_t col, uint8_t datum = top_left) {
  c.setFont(&fonts::Font0);  // 8 px tall, the smallest allowed
  c.setTextDatum(datum);
  c.setTextColor(rgb(col));
  c.drawString(s, x, y);
}

// A little key drawn like a keyboard key, e.g. [tab]. Returns its width.
static int keycap(M5Canvas& c, const char* k, int x, int y) {
  int w = strlen(k) * 6 + 5;
  c.fillRoundRect(x, y, w, 11, 2, rgb(0x26363F));
  c.drawRoundRect(x, y, w, 11, 2, rgb(0x41596A));
  text(c, k, x + w / 2, y + 2, C_WHITE, top_center);
  return w;
}

static void keyHint(M5Canvas& c, const char* k, const char* label, int x, int y) {
  int w = keycap(c, k, x, y);
  text(c, label, x + w + 5, y + 2, C_MUTED);
}

// ---------- background, board and panel ----------
static void drawBackground(M5Canvas& c) {
  for (int y = 0; y < 135; y++) c.drawFastHLine(0, y, 240, rgb(mix(0x071019, 0x0A1822, y / 134.0f)));
}

// Sea colour at screen row y (the sea is a top-to-bottom gradient).
static uint32_t seaAt(int y, bool fog) {
  float k = (y - GY) / float(N * CS);
  return fog ? mix(0x0B2438, 0x061522, k) : mix(0x11507F, 0x0A3256, k);
}

static void drawLabels(M5Canvas& c, int cx, int cy) {
  char buf[4];
  for (int i = 0; i < N; i++) {
    buf[0] = COLS[i];
    buf[1] = 0;
    text(c, buf, GX + i * CS + 6, GY - 10, i == cx ? C_YELLOW : C_LABEL, top_center);
    snprintf(buf, sizeof buf, "%d", i + 1);
    text(c, buf, GX - 2, GY + i * CS + 2, i == cy ? C_YELLOW : C_LABEL, top_right);
  }
}

static void drawSea(M5Canvas& c, uint32_t t, bool fog) {
  const int size = N * CS;
  for (int y = GY; y <= GY + size; y++) c.drawFastHLine(GX, y, size + 1, rgb(seaAt(y, fog)));

  // Little wave strokes that drift and blink, one per square.
  const uint32_t waveCol = fog ? 0x78AAD2 : 0xAFDCF8;
  const float waveAlpha = fog ? 0.13f : 0.25f;
  for (int y = 0; y < N; y++) {
    uint16_t col = rgb(mix(seaAt(GY + y * CS + 5, fog), waveCol, waveAlpha * 1.6f));
    for (int x = 0; x < N; x++) {
      float ph = t / 700.0f + x * 0.9f + y * 1.7f;
      if (sinf(ph) < -0.1f) continue;
      int ox = lroundf(sinf(ph * 0.5f) * 1.8f);
      int px = GX + x * CS + ox, py = GY + y * CS;
      c.drawLine(px + 2, py + 6, px + 4, py + 5, col);
      c.drawLine(px + 4, py + 5, px + 6, py + 6, col);
    }
  }

  // Faint grid lines, then a brighter border.
  const uint32_t lineCol = fog ? 0x5082AA : 0x96C8E6;
  for (int i = 0; i <= N; i++) {
    int y = GY + i * CS;
    c.drawFastHLine(GX, y, size + 1, rgb(mix(seaAt(y, fog), lineCol, 0.16f)));
    for (int row = 0; row < N; row++) {  // vertical lines in short pieces so they follow the gradient
      int y0 = GY + row * CS;
      c.drawFastVLine(GX + i * CS, y0, CS, rgb(mix(seaAt(y0 + 5, fog), lineCol, 0.16f)));
    }
  }
  c.drawRect(GX - 1, GY - 1, size + 3, size + 3, rgb(mix(0x08141D, 0xAAD7F5, 0.35f)));
}

// Pulsing corner brackets around the square under the cursor.
static void drawCursor(M5Canvas& c, int cx, int cy, uint32_t t) {
  int px = GX + cx * CS, py = GY + cy * CS;
  c.fillRect(px + 1, py + 1, CS - 1, CS - 1, rgb(mix(seaAt(py + 5, false), C_YELLOW, 0.18f)));

  int p = lroundf((sinf(t / 160.0f) + 1) / 2 * 1.5f);  // 0..2 px breathing
  int x0 = px - p, y0 = py - p, s = CS + 2 * p, k = 4;
  uint16_t col = rgb(C_YELLOW);
  for (int w = 0; w < 2; w++) {  // 2 px thick, growing inwards
    int a = x0 + w, b = y0 + w, e = s - 2 * w;
    c.drawFastHLine(a, b, k, col);              c.drawFastVLine(a, b, k, col);              // top left
    c.drawFastHLine(a + e - k + 1, b, k, col);  c.drawFastVLine(a + e, b, k, col);          // top right
    c.drawFastHLine(a, b + e, k, col);          c.drawFastVLine(a, b + e - k + 1, k, col);  // bottom left
    c.drawFastHLine(a + e - k + 1, b + e, k, col);
    c.drawFastVLine(a + e, b + e - k + 1, k, col);                                          // bottom right
  }
}

// Rounded side panel with a dark top-to-bottom gradient.
static void drawPanel(M5Canvas& c) {
  const int x = PX - 5, y = 2, w = 240 - PX + 3, h = 131, r = 5;
  for (int i = 0; i < h; i++) {
    int inset = 0;
    int d = i < r ? r - i : (i >= h - r ? i - (h - r - 1) : 0);  // rows into a rounded corner
    if (d > 0) inset = r - lroundf(sqrtf(float(r * r - (d - 0.5f) * (d - 0.5f))));
    c.drawFastHLine(x + inset, y + i, w - 2 * inset, rgb(mix(0x13232D, 0x0B161D, (y + i) / 135.0f)));
  }
  c.drawRoundRect(x, y, w, h, r, rgb(mix(0x101D25, 0x8CBED7, 0.12f)));
}

static void drawPanelContents(M5Canvas& c, const Screen& s) {
  c.setFont(&fonts::FreeSansBold9pt7b);
  c.setTextDatum(baseline_left);
  c.setTextColor(rgb(C_YELLOW));
  c.drawString("TARGET", PX, 19);

  text(c, s.fog ? "your shots" : "your fleet", PX, 29, C_MUTED);
  keycap(c, "tab", PX + 80, 27);

  char coord[4];
  snprintf(coord, sizeof coord, "%c%d", COLS[s.cursorX], s.cursorY + 1);
  c.setFont(&fonts::FreeSansBold12pt7b);
  c.setTextDatum(baseline_left);
  c.setTextColor(rgb(C_YELLOW));
  c.drawString(coord, PX, 62);

  text(c, "arrows  move", PX, 74, C_MUTED);
  keyHint(c, "tab", "view", PX, 86);
  keyHint(c, "ok", "beep", PX, 100);

  char fps[12];
  snprintf(fps, sizeof fps, "fps %d", s.fps);
  text(c, fps, PX, 122, C_LABEL);
}

void drawFrame(M5Canvas& c, const Screen& s, uint32_t t) {
  drawBackground(c);
  drawLabels(c, s.cursorX, s.cursorY);
  drawSea(c, t, s.fog);
  drawCursor(c, s.cursorX, s.cursorY, t);
  drawPanel(c);
  drawPanelContents(c, s);
}
