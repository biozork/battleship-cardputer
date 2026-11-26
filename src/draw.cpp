#include "draw.h"
#include "fonts.h"
#include "sound.h"

// Layout, same numbers as the mock: grid at (GX,GY), 11 px cells, panel at PX.
static const int N = GRID_N, CS = 11, GX = 16, GY = 12, PX = 134, PANEL_RIGHT = PX + 103;
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

static const uint32_t C_YELLOW = 0xFFD43B, C_WHITE = 0xE8EEF0, C_MUTED = 0x9FB3BD, C_LABEL = 0x6F8794,
                      C_GREEN = 0x4CD27A;

// ---------- see-through drawing ----------
// The mock uses see-through colours a lot. These helpers blend straight into
// the canvas memory. Pixels there are stored byte-swapped, ready for the display.
static uint16_t* fb = nullptr;  // the canvas pixels, set at the start of each frame
static int clipL = 0, clipT = 0, clipR = 240, clipB = 135;  // blending stays inside this box

static void setClip(M5Canvas& c, int x, int y, int w, int h) {
  c.setClipRect(x, y, w, h);
  clipL = x, clipT = y, clipR = x + w, clipB = y + h;
}

static void clearClip(M5Canvas& c) {
  c.clearClipRect();
  clipL = 0, clipT = 0, clipR = 240, clipB = 135;
}

static int alpha256(float a) { return a <= 0 ? 0 : a >= 1 ? 256 : int(a * 256); }

static inline void blendPixel(int x, int y, uint32_t col, int a) {  // a = 0..256
  if (x < clipL || y < clipT || x >= clipR || y >= clipB) return;
  uint16_t& px = fb[y * 240 + x];
  uint16_t p = __builtin_bswap16(px);
  int r = (p >> 8) & 0xF8, g = (p >> 3) & 0xFC, b = (p << 3) & 0xF8;
  r += (((int)(col >> 16) - r) * a) >> 8;
  g += (((int)((col >> 8) & 0xFF) - g) * a) >> 8;
  b += (((int)(col & 0xFF) - b) * a) >> 8;
  px = __builtin_bswap16(rgb((r << 16) | (g << 8) | b));
}

static void blendRect(int x, int y, int w, int h, uint32_t col, float alpha) {
  int a = alpha256(alpha);
  if (!fb || !a) return;
  for (int j = y; j < y + h; j++)
    for (int i = x; i < x + w; i++) blendPixel(i, j, col, a);
}

static void blendCircle(float cx, float cy, float r, uint32_t col, float alpha) {
  int a = alpha256(alpha);
  if (!fb || !a || r <= 0) return;
  for (int j = floorf(cy - r); j <= ceilf(cy + r); j++)
    for (int i = floorf(cx - r); i <= ceilf(cx + r); i++)
      if ((i - cx) * (i - cx) + (j - cy) * (j - cy) <= r * r) blendPixel(i, j, col, a);
}

// Outline of an ellipse, see-through.
static void blendEllipse(float cx, float cy, float rx, float ry, uint32_t col, float alpha) {
  int a = alpha256(alpha);
  if (!fb || !a) return;
  int steps = 8 + int(rx * 5);
  for (int i = 0; i < steps; i++) {
    float ang = i * 2 * PI / steps;
    blendPixel(lroundf(cx + cosf(ang) * rx), lroundf(cy + sinf(ang) * ry), col, a);
  }
}

// ---------- text ----------
// Smooth fonts, made by tools/make_fonts.py from the mock's two typefaces:
// Atkinson Hyperlegible for reading, Pixelify Sans for headings and big text.
enum class Font { Body, Bold, Small, Head, Logo, Big, Banner, Title, COUNT };

struct SmoothFont {
  lgfx::PointerWrapper data;  // where the font's bytes are (in flash)
  lgfx::VLWfont font;         // the font, ready to draw with
};
static SmoothFont smooth[(int)Font::COUNT];

// Prepares each font once, so drawing text later is quick.
static void loadFonts() {
  static bool done = false;
  if (done) return;
  done = true;
  const uint8_t* arrays[] = {FONT_BODY, FONT_BOLD, FONT_SMALL, FONT_HEAD, FONT_LOGO, FONT_BIG, FONT_BANNER, FONT_TITLE};
  const size_t sizes[] = {sizeof FONT_BODY, sizeof FONT_BOLD, sizeof FONT_SMALL, sizeof FONT_HEAD,
                          sizeof FONT_LOGO, sizeof FONT_BIG,  sizeof FONT_BANNER, sizeof FONT_TITLE};
  for (int i = 0; i < (int)Font::COUNT; i++) {
    smooth[i].data.set(arrays[i], sizes[i]);
    smooth[i].font.loadFont(&smooth[i].data);
  }
}

// Draws text. y is the baseline (the line the letters sit on), like in the mock.
static void text(M5Canvas& c, const char* s, int x, int y, uint32_t col, Font f = Font::Body,
                 uint8_t datum = baseline_left) {
  c.setFont(&smooth[(int)f].font);
  c.setTextSize(1);
  c.setTextDatum(datum);
  c.setTextColor(rgb(col));
  c.drawString(s, x, y);
}

static int textWidth(M5Canvas& c, const char* s, Font f) {
  c.setFont(&smooth[(int)f].font);
  c.setTextSize(1);
  return c.textWidth(s);
}

static void heading(M5Canvas& c, const char* s, int x, int y, uint32_t col, uint8_t datum = baseline_left) {
  text(c, s, x, y, col, Font::Head, datum);
}

// A little key drawn like a keyboard key, e.g. [tab]. y is its top; it is 11 px tall.
static int keycapWidth(M5Canvas& c, const char* k) { return max(10, textWidth(c, k, Font::Small) + 6); }

static int keycap(M5Canvas& c, const char* k, int x, int y) {
  int w = keycapWidth(c, k);
  c.fillRoundRect(x, y, w, 11, 2, rgb(0x26363F));
  c.drawRoundRect(x, y, w, 11, 2, rgb(0x41596A));
  text(c, k, x + w / 2, y + 9, C_WHITE, Font::Small, baseline_center);
  return w;
}

// A key followed by what it does, e.g. [ok] fire.
static int keyHintWidth(M5Canvas& c, const char* k, const char* label) {
  return keycapWidth(c, k) + 4 + textWidth(c, label, Font::Body);
}

static void keyHint(M5Canvas& c, const char* k, const char* label, int x, int y) {
  int w = keycap(c, k, x, y);
  text(c, label, x + w + 4, y + 9, C_MUTED);
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

// Column letters and row numbers. The cursor's column and row light up (-1 = none).
static void drawLabels(M5Canvas& c, int cx, int cy) {
  char buf[4];
  for (int i = 0; i < N; i++) {
    buf[0] = COLS[i];
    buf[1] = 0;
    text(c, buf, GX + i * CS + 6, GY - 3, i == cx ? C_YELLOW : C_LABEL, Font::Small, baseline_center);
    snprintf(buf, sizeof buf, "%d", i + 1);
    text(c, buf, GX - 3, GY + i * CS + 9, i == cy ? C_YELLOW : C_LABEL, Font::Small, baseline_right);
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
  blendRect(px + 1, py + 1, CS - 1, CS - 1, C_YELLOW, 0.18f);

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

// ---------- ships ----------
enum class Look { Normal, Sunk, GhostOk, GhostBad };

// A hull with a pointed bow, a bridge in the middle and guns.
// The hull is drawn as 9 lines along the ship; "along" runs stern to bow.
// Horizontal ships point right, vertical ships point up (as in the mock).
// alpha only applies to the see-through ghost.
static void drawShip(M5Canvas& c, const Ship& s, Look look, float alpha = 1) {
  const int L = s.size * CS, x0 = GX + s.x * CS, y0 = GY + s.y * CS;
  const bool h = s.horiz;
  static const int8_t STERN[5] = {0, 0, 0, 0, 1};  // rounding at the stern corners
  static const int8_t BOW[5] = {0, 0, 1, 2, 4};    // taper towards the bow tip
  int from[9], to[9];
  for (int a = 0; a < 9; a++) {
    int d = abs(a - 4);
    from[a] = 2 + STERN[d];
    to[a] = L - 1 - BOW[d];
  }
  const bool ghost = look == Look::GhostOk || look == Look::GhostBad;
  auto span = [&](int a, int u0, int u1, uint32_t col, float al) {
    int n = u1 - u0 + 1;
    if (n <= 0) return;
    if (ghost) {
      if (h) blendRect(x0 + u0, y0 + 2 + a, n, 1, col, al);
      else blendRect(x0 + 2 + a, y0 + L - 1 - u1, 1, n, col, al);
    } else if (h) {
      c.drawFastHLine(x0 + u0, y0 + 2 + a, n, rgb(col));
    } else {
      c.drawFastVLine(x0 + 2 + a, y0 + L - 1 - u1, n, rgb(col));
    }
  };

  uint32_t edge;
  if (look == Look::GhostOk) edge = 0x9FF5C0;
  else if (look == Look::GhostBad) edge = 0xFF9A9A;
  else if (look == Look::Sunk) edge = 0x140807;
  else edge = 0x25313D;
  for (int a = 0; a < 9; a++) {
    // The inside of this line; the rest is outline. The outline is 1 px, also
    // where the neighbouring lines are shorter. The first and last lines are all outline.
    int u0 = to[a] + 1, u1 = to[a];
    if (a > 0 && a < 8) {
      u0 = max(from[a] + 1, max(from[a - 1], from[a + 1]));
      u1 = min(to[a] - 1, min(to[a - 1], to[a + 1]));
      if (u0 > u1) u0 = to[a] + 1, u1 = to[a];
    }
    span(a, from[a], u0 - 1, edge, alpha);
    span(a, u1 + 1, to[a], edge, alpha);
    if (u0 > u1) continue;
    float k = a / 8.0f;
    uint32_t fill;
    if (look == Look::GhostOk) fill = C_GREEN;
    else if (look == Look::GhostBad) fill = 0xF03E3E;
    else if (look == Look::Sunk) fill = mix(0x6A3128, 0x2A1210, k);
    else fill = k < 0.45f ? mix(0xC9D5DF, 0x8C9DAF, k / 0.45f) : mix(0x8C9DAF, 0x52627A, (k - 0.45f) / 0.55f);
    span(a, u0, u1, fill, 0.45f * alpha);
  }
  if (ghost) return;

  const bool sunk = look == Look::Sunk;
  const int n = s.size;
  for (int i = 0; i < n; i++) {
    int cx = h ? x0 + i * CS + 6 : x0 + 6, cy = h ? y0 + 6 : y0 + i * CS + 6;
    if (i == n / 2) {  // bridge
      c.fillRect(cx - 2, cy - 2, 5, 5, rgb(sunk ? 0x3B1A15 : 0xDFE8EF));
      c.fillRect(cx - 1, cy - 1, 3, 3, rgb(sunk ? 0x1E0C0A : 0x4B5D70));
    } else if (i != n - 1 || n == 2) {  // gun turret with a barrel pointing to the bow
      uint16_t col = rgb(sunk ? 0x2E1410 : 0x3A4A5C);
      c.fillRect(cx - 1, cy - 1, 3, 3, col);
      if (h) c.drawFastHLine(cx, cy, 4, col);
      else c.drawFastVLine(cx, cy - 3, 4, col);
    }
  }
}

// ---------- hit and miss markers ----------
// A burning square: a soft glow and three flickering flame layers. small = an older, dying fire.
static void drawFire(M5Canvas& c, int x, int y, uint32_t t, bool small, bool fog) {
  const float s = small ? 0.6f : 1.0f;
  int cx = GX + x * CS + 6, cy = GY + y * CS + 7;
  c.fillCircle(cx, cy - 1, lroundf(4 * s), rgb(mix(seaAt(cy, fog), 0xFF8C32, 0.4f)));
  static const uint32_t cols[3] = {0xFF4D1A, 0xFF9F1C, 0xFFE066};
  for (int i = 0; i < 3; i++) {
    float f = sinf(t / 85.0f + i * 2.1f + x * 3 + y * 5);
    float hgt = (4.5f + f * 1.4f - i * 0.9f) * s, w = (3.2f - i * 0.85f) * s;
    c.fillTriangle(lroundf(cx - w), cy + 1, lroundf(cx + w), cy + 1, lroundf(cx + f * 0.7f), lroundf(cy - hgt),
                   rgb(cols[i]));
  }
}

// A miss: a white ring on the water, gently breathing.
static void drawMiss(M5Canvas& c, int x, int y, uint32_t t, bool fog) {
  int cx = GX + x * CS + 6, cy = GY + y * CS + 6;
  int b = sinf(t / 500.0f + x + y) > 0.3f ? 1 : 0;
  c.drawEllipse(cx, cy, 3 + b, 2 + b, rgb(mix(seaAt(cy, fog), 0xDCECF6, 0.75f)));
  c.fillRect(cx - 1, cy - 1, 2, 2, rgb(0xEAF4FA));
}

// Two puffs of smoke drifting up from a wreck.
static void drawSmoke(int x, int y, uint32_t t) {
  for (int i = 0; i < 2; i++) {
    float ph = fmodf(t / 1500.0f + i * 0.5f + ((x * 7 + y * 13) % 10) / 10.0f, 1);
    float cx = GX + x * CS + 5.5f + sinf(ph * 6 + x) * 1.5f, cy = GY + y * CS + 5.5f - ph * 12;
    blendCircle(cx, cy, 1.5f + ph * 3, 0x96A0AC, 0.4f * (1 - ph));
  }
}

// Small ship shape for the side panel. y is its top; it is 6 px tall.
static void miniHull(M5Canvas& c, int x, int y, int len, uint32_t col1, uint32_t col2) {
  static const int8_t TIP[6] = {0, 1, 2, 2, 1, 0};
  for (int r = 0; r < 6; r++) c.drawFastHLine(x, y + r, len - 3 + TIP[r], rgb(mix(col1, col2, r / 5.0f)));
}

// ---------- side panel ----------
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

static void panelPlace(M5Canvas& c, const Game& g) {
  const ShipType& type = SHIP_TYPES[g.fleet.count];
  heading(c, "PLACE FLEET", PX, 17, C_YELLOW);
  text(c, type.name, PX, 33, 0xFFFFFF, Font::Bold);
  miniHull(c, PX, 38, type.size * 10, 0xC9D5DF, 0x52627A);

  for (int i = 0; i < NUM_SHIPS; i++) {  // progress: placed, current, still to come
    uint32_t col = i < g.fleet.count ? C_GREEN : i == g.fleet.count ? C_YELLOW : 0x2C3E48;
    c.fillCircle(PX + 3 + i * 9, 54, 2, rgb(col));
  }

  keyHint(c, "r", "rotate", PX, 64);
  keyHint(c, "a", "auto", PX, 78);
  keyHint(c, "ok", "place", PX, 92);
  if (g.msg[0][0]) text(c, g.msg[0], PX, 125, C_GREEN);
}

static void panelWait(M5Canvas& c, uint32_t t) {
  heading(c, "FLEET READY", PX, 17, C_GREEN);
  text(c, "Waiting for", PX, 36, C_WHITE);
  text(c, "partner", PX, 48, C_WHITE);
  int dx = PX + textWidth(c, "partner", Font::Body) + 4;
  for (int i = 0; i < 3; i++) {
    float a = (sinf(t / 200.0f - i * 0.7f) + 1) / 2;
    c.fillCircle(dx + i * 6, 46, 1, rgb(mix(0x0F1D25, C_GREEN, 0.3f + 0.7f * a)));
  }
  text(c, "First one ready", PX, 72, C_MUTED);
  text(c, "fires first.", PX, 84, C_MUTED);
}

static void panelBattle(M5Canvas& c, const Game& g, uint32_t t) {
  const bool live = g.myTurn && !g.pending;  // waiting for us to fire
  float pulse = (sinf(t / 250.0f) + 1) / 2;
  uint32_t banner = live ? mix(0x2B8A3E, 0x3CB955, pulse) : g.myTurn ? 0x2B8A3E : 0x2A3A44;
  c.fillRoundRect(PX - 1, 6, 101, 17, 4, rgb(banner));
  if (live) c.drawRoundRect(PX - 2, 5, 103, 19, 5, rgb(mix(0x13232D, C_GREEN, 0.3f + 0.7f * pulse)));
  heading(c, g.myTurn ? (g.pending ? "FIRING..." : "YOUR TURN") : "THEIR TURN", PX + 49, 19, 0xFFFFFF,
          baseline_center);

  text(c, g.view == View::Shots ? "your shots" : "your fleet", PX, 36, C_MUTED);
  keycap(c, "tab", PANEL_RIGHT - keycapWidth(c, "tab"), 27);

  if (live && g.view == View::Shots) {
    char coord[4];
    coordName(g.cursorX, g.cursorY, coord);
    text(c, coord, PX, 58, C_YELLOW, Font::Big);
  }
  text(c, g.msg[0], PX, 71, C_WHITE);
  text(c, g.msg[1], PX, 83, C_YELLOW, Font::Bold);

  // ships left: theirs and ours; sunk ones turn dark red
  text(c, "them", PX, 97, C_MUTED);
  for (int i = 0; i < NUM_SHIPS; i++) {
    bool dead = i < g.enemySunkCount;
    miniHull(c, PX + 30 + i * 14, 90, 11, dead ? 0x6A3128 : 0xC9D5DF, dead ? 0x2A1210 : 0x52627A);
  }
  int left = g.fleet.afloat();
  text(c, "you", PX, 110, C_MUTED);
  for (int i = 0; i < NUM_SHIPS; i++) {
    bool dead = i >= left;
    miniHull(c, PX + 30 + i * 14, 103, 11, dead ? 0x6A3128 : 0xC9D5DF, dead ? 0x2A1210 : 0x52627A);
  }

  char score[16];
  snprintf(score, sizeof score, "score %u-%u", g.wins, g.losses);
  text(c, score, PX, 126, C_MUTED);
  if (live) keyHint(c, "ok", "fire", PANEL_RIGHT - keyHintWidth(c, "ok", "fire"), 117);
}

// ---------- board views ----------
static void drawFleetView(M5Canvas& c, const Game& g, uint32_t t) {
  drawSea(c, t, false);
  for (int i = 0; i < g.fleet.count; i++) {
    const Ship& s = g.fleet.ships[i];
    drawShip(c, s, s.sunk() ? Look::Sunk : Look::Normal);
  }
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      if (g.incoming[y][x] == MARK_MISS) drawMiss(c, x, y, t, false);
      else if (g.incoming[y][x] == MARK_HIT) {
        int s = g.fleet.shipAt(x, y);
        bool dead = s >= 0 && g.fleet.ships[s].sunk();
        drawFire(c, x, y, t, dead, false);
        if (dead) drawSmoke(x, y, t);
      }
    }
}

static void drawShotsView(M5Canvas& c, const Game& g, uint32_t t) {
  drawSea(c, t, true);
  for (int i = 0; i < g.enemySunkCount; i++) drawShip(c, g.enemySunk[i], Look::Sunk);
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      uint8_t m = g.shots[y][x];
      if (m == MARK_MISS) drawMiss(c, x, y, t, true);
      else if (m == MARK_HIT) drawFire(c, x, y, t, false, true);
      else if (m == MARK_SUNK) {
        drawFire(c, x, y, t, true, true);
        drawSmoke(x, y, t);
      }
    }
}

// ---------- effects: explosions, splashes, banners, shake ----------
enum class FxType : uint8_t { Boom, Splash, Banner };
struct Effect {
  bool active;
  FxType type;
  int8_t x, y;
  View layer;  // which view it belongs to
  uint32_t start, dur;
  float seed;
  const char* text;
  uint32_t col;
};
static Effect effects[8];
static uint32_t shakeStart = 0;
static bool aimOn = false;
static int8_t aimX, aimY;
static uint32_t aimStart;

static void addEffect(const Effect& e) {
  for (auto& slot : effects)
    if (!slot.active) {
      slot = e;
      return;
    }
  effects[0] = e;  // all busy: replace one
}

namespace Fx {
void boom(int x, int y, View layer, uint32_t t) {
  addEffect({true, FxType::Boom, (int8_t)x, (int8_t)y, layer, t, 700, random(600) / 100.0f, nullptr, 0});
}
void splash(int x, int y, View layer, uint32_t t) {
  addEffect({true, FxType::Splash, (int8_t)x, (int8_t)y, layer, t, 750, 0, nullptr, 0});
}
void banner(const char* text, uint32_t col, uint32_t t) {
  addEffect({true, FxType::Banner, 0, 0, View::Shots, t, 1500, 0, text, col});
}
void shake(uint32_t t) { shakeStart = t; }
void aim(int x, int y, uint32_t t) {
  aimOn = true;
  aimX = x;
  aimY = y;
  aimStart = t;
}
void clearAim() { aimOn = false; }
void clear() {
  for (auto& e : effects) e.active = false;
  aimOn = false;
  shakeStart = 0;
}
}  // namespace Fx

static void drawBoom(const Effect& e, float k) {
  float cx = GX + e.x * CS + 6, cy = GY + e.y * CS + 6, r = 3 + k * 14;
  // a fireball: white-hot centre, orange, fading to nothing at the edge
  for (int j = floorf(cy - r); j <= ceilf(cy + r); j++)
    for (int i = floorf(cx - r); i <= ceilf(cx + r); i++) {
      float u = sqrtf((i - cx) * (i - cx) + (j - cy) * (j - cy)) / r;
      if (u > 1) continue;
      uint32_t col;
      float a;
      if (u < 0.4f) {
        col = mix(0xFFFAD7, 0xFF9628, u / 0.4f);
        a = (1 - k) * (1 - 0.1f * u / 0.4f);
      } else {
        col = mix(0xFF9628, 0xFF3C00, (u - 0.4f) / 0.6f);
        a = 0.9f * (1 - k) * (1 - (u - 0.4f) / 0.6f);
      }
      blendPixel(i, j, col, alpha256(a));
    }
  for (int i = 0; i < 12; i++) {  // sparks flying out and falling
    float ang = i / 12.0f * 2 * PI + e.seed, v = 9 + (i % 3) * 5;
    float dx = cosf(ang) * v * k, dy = sinf(ang) * v * k + 16 * k * k;
    uint32_t col = 0xFF0046 | ((190 - i * 9) << 8);
    blendRect(lroundf(cx + dx) - 1, lroundf(cy + dy) - 1, 2, 2, col, 1 - k);
  }
}

static void drawSplash(const Effect& e, float k) {
  float cx = GX + e.x * CS + 6, cy = GY + e.y * CS + 6;
  for (float o : {0.0f, 0.25f}) {  // two ripples, the second a moment later
    float kk = k - o;
    if (kk > 0) blendEllipse(cx, cy, 2 + kk * 10, 1 + kk * 5.5f, 0xE1F2FF, 1 - k);
  }
  for (int i = 0; i < 7; i++) {  // droplets thrown up and falling back
    float ang = -PI / 2 + (i - 3) * 0.33f;
    float dx = cosf(ang) * 13 * k, dy = sinf(ang) * 13 * k + 24 * k * k;
    blendRect(lroundf(cx + dx) - 1, lroundf(cy + dy) - 1, 2, 2, 0xE6F5FF, 1 - k);
  }
}

// "SUNK!" sliding in from the left, pausing, and sliding out to the right.
static void drawBanner(M5Canvas& c, const Effect& e, float k) {
  float in = k < 0.18f ? k / 0.18f : k > 0.82f ? (1 - k) / 0.18f : 1;
  float ease = 1 - powf(1 - in, 3);
  int cy = GY + N * CS / 2, w = N * CS;
  int off = lroundf((1 - ease) * (k < 0.5f ? -50 : 50));
  blendRect(GX, cy - 13, w + 1, 26, 0x04090D, 0.8f * ease);
  blendRect(GX, cy - 13, w + 1, 2, e.col, ease);
  blendRect(GX, cy + 11, w + 1, 2, e.col, ease);
  setClip(c, GX, cy - 13, w + 1, 26);
  text(c, e.text, GX + w / 2 + off, cy + 7, mix(0x0A1218, e.col, ease), Font::Banner, baseline_center);
  clearClip(c);
}

static void drawEffects(M5Canvas& c, const Game& g, uint32_t t) {
  for (auto& e : effects) {
    if (!e.active) continue;
    int32_t el = (int32_t)(t - e.start);
    if (el >= (int32_t)e.dur) {
      e.active = false;
      continue;
    }
    if (el < 0) continue;  // starts later
    float k = el / float(e.dur);
    if (e.type == FxType::Banner) {
      drawBanner(c, e, k);
      continue;
    }
    // only show it on the view it happened on (after the game, fleet effects always show)
    if (e.layer != g.view && !(g.phase != Phase::Battle && e.layer == View::Fleet)) continue;
    if (e.type == FxType::Boom) drawBoom(e, k);
    else drawSplash(e, k);
  }
}

// The target ring that shrinks onto the square we fired at.
static void drawAim(M5Canvas& c, uint32_t t) {
  float k = min(1.0f, (t - aimStart) / 260.0f);
  int cx = GX + aimX * CS + 6, cy = GY + aimY * CS + 6, r = lroundf(4 + 12 * (1 - k));
  uint16_t col = rgb(mix(0x0B2438, C_YELLOW, 0.9f));
  c.drawCircle(cx, cy, r, col);
  c.drawFastHLine(cx - r - 2, cy, 5, col);
  c.drawFastHLine(cx + r - 2, cy, 5, col);
  c.drawFastVLine(cx, cy - r - 2, 5, col);
  c.drawFastVLine(cx, cy + r - 2, 5, col);
}

void present(M5Canvas& c, uint32_t t) {
  int ox = 0;
  int32_t el = (int32_t)(t - shakeStart);
  if (shakeStart && el >= 0 && el < 380) ox = lroundf(sinf(el / 15.0f) * 3 * (1 - el / 380.0f));
  c.pushSprite(ox, 0);
  // fill the strip the shifted picture leaves uncovered
  if (ox > 0) M5Cardputer.Display.fillRect(0, 0, ox, 135, rgb(0x071019));
  if (ox < 0) M5Cardputer.Display.fillRect(240 + ox, 0, -ox, 135, rgb(0x071019));
}

// ---------- game over ----------
// Blend every pixel already drawn towards one colour (k = 0..1), like a see-through overlay.
static void tintScreen(M5Canvas& c, uint32_t col, float k) {
  blendRect(0, 0, 240, 135, col, k);
}

static void fireworks(M5Canvas& c, uint32_t t) {
  static const uint32_t cols[4] = {C_YELLOW, C_GREEN, 0x74C0FC, 0xFF8787};
  for (int b = 0; b < 3; b++) {
    const uint32_t per = 1600, tt = t + b * 530;
    float ph = (tt % per) / float(per);
    uint32_t seed = (tt / per) * 3 + b;
    int cx = 40 + (seed * 73) % 160, cy = 22 + (seed * 41) % 40;
    uint16_t col = rgb(mix(0x05090D, cols[seed % 4], 1 - ph));
    for (int i = 0; i < 16; i++) {
      float a = i / 16.0f * 2 * PI, r = ph * 24;
      c.fillRect(cx + cosf(a) * r - 1, cy + sinf(a) * r + ph * ph * 10 - 1, 2, 2, col);
    }
  }
}

static void drawOver(M5Canvas& c, const Game& g, uint32_t t) {
  if (g.won) tintScreen(c, 0x04090D, 0.8f);
  else tintScreen(c, 0x1E0606, 0.82f);
  if (g.won) fireworks(c, t);

  int bob = lroundf(sinf(t / 300.0f) * 1.5f);
  text(c, g.won ? "YOU WIN!" : "FLEET SUNK", 120, 54 + bob, g.won ? C_YELLOW : 0xFF8787, Font::Title,
       baseline_center);
  text(c, g.won ? "Every ship found." : "Good game, admiral.", 120, 73, C_WHITE, Font::Body, baseline_center);
  char score[24];
  snprintf(score, sizeof score, "score %u - %u", g.wins, g.losses);
  text(c, score, 120, 89, C_MUTED, Font::Body, baseline_center);

  if (g.wantRematch) {
    char wait[28];
    snprintf(wait, sizeof wait, "waiting for partner%.*s", int(t / 400 % 4), "...");
    int w = textWidth(c, "waiting for partner...", Font::Body);  // fixed start, so the dots don't wobble
    text(c, wait, 120 - w / 2, 114, C_GREEN);
  } else if (sinf(t / 250.0f) > -0.5f) {  // blinking hint
    keyHint(c, "ok", "rematch", 120 - keyHintWidth(c, "ok", "rematch") / 2, 105);
  }
}

// ---------- search screen ----------
static void drawSearch(M5Canvas& c, uint32_t t, const char* myName, bool lost) {
  drawBackground(c);
  const int cx = 58, cy = 68, r = 46;
  const uint32_t BRIGHT = 0x7CF2A4, DISC = 0x0A2016;
  float a = fmodf(t / 650.0f, 2 * PI);

  for (int rr = r; rr > 0; rr--) c.fillCircle(cx, cy, rr, rgb(mix(0x0D2A1C, 0x06140E, rr / float(r))));
  for (int i = 0; i < 48; i++) {  // fading trail behind the sweep
    float ang = a - 1.2f + i * 0.025f;
    c.drawLine(cx, cy, cx + cosf(ang) * (r - 1), cy + sinf(ang) * (r - 1), rgb(mix(DISC, C_GREEN, 0.35f * i / 47)));
  }
  uint16_t ring = rgb(mix(DISC, C_GREEN, 0.35f));
  for (int i = 1; i <= 3; i++) c.drawCircle(cx, cy, r * i / 3, ring);
  c.drawFastHLine(cx - r, cy, 2 * r + 1, ring);
  c.drawFastVLine(cx, cy - r, 2 * r + 1, ring);
  c.drawLine(cx, cy, cx + cosf(a) * r, cy + sinf(a) * r, rgb(BRIGHT));

  const float blips[4][2] = {{0.7f, 30}, {2.6f, 20}, {4.2f, 36}, {5.4f, 12}};
  for (auto& b : blips) {  // blips light up as the sweep passes, then fade
    float diff = fmodf(fmodf(a - b[0], 2 * PI) + 2 * PI, 2 * PI);
    float al = 1 - diff / 2.4f;
    if (al <= 0) continue;
    c.fillCircle(cx + cosf(b[0]) * b[1], cy + sinf(b[0]) * b[1], 2, rgb(mix(DISC, BRIGHT, al)));
  }

  text(c, "BATTLESHIP", 114, 40, C_YELLOW, Font::Logo);
  text(c, "two-player, ESP-NOW", 115, 54, C_MUTED);
  if (lost) text(c, "partner lost!", 115, 70, 0xFF8787, Font::Bold);
  text(c, "searching for", 115, 84, C_WHITE);
  char dots[12];
  snprintf(dots, sizeof dots, "partner%.*s", int(t / 400 % 4), "...");
  text(c, dots, 115, 96, C_WHITE);
  keyHint(c, "m", Sound::on() ? "sound on" : "sound off", 115, 104);
  text(c, myName, 115, 128, C_LABEL);
}

// ---------- whole frame ----------
void drawGame(M5Canvas& c, const Game& g, uint32_t t, const char* myName, bool lost) {
  fb = (uint16_t*)c.getBuffer();
  loadFonts();
  if (g.phase == Phase::Search) {
    drawSearch(c, t, myName, lost);
    return;
  }

  drawBackground(c);
  const bool placing = g.phase == Phase::Place;
  const bool battle = g.phase == Phase::Battle || g.phase == Phase::Over;
  const bool aiming = g.phase == Phase::Battle && g.view == View::Shots && g.myTurn && !g.pending;
  drawLabels(c, placing || aiming ? g.cursorX : -1, placing || aiming ? g.cursorY : -1);

  setClip(c, GX, GY, N * CS + 1, N * CS + 1);  // anything hanging off the board gets cut off
  if (battle && g.view == View::Shots) drawShotsView(c, g, t);
  else drawFleetView(c, g, t);
  if (placing) {
    Ship gh = g.ghost();
    drawShip(c, gh, g.fleet.fits(gh) ? Look::GhostOk : Look::GhostBad, 0.8f + 0.2f * sinf(t / 200.0f));
  }
  clearClip(c);
  if (aiming) drawCursor(c, g.cursorX, g.cursorY, t);
  if (aimOn && g.pending && g.view == View::Shots) drawAim(c, t);
  drawEffects(c, g, t);

  drawPanel(c);
  if (placing) panelPlace(c, g);
  else if (g.phase == Phase::Waiting) panelWait(c, t);
  else panelBattle(c, g, t);

  if (g.phase == Phase::Over) drawOver(c, g, t);
}
