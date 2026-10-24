#include "draw.h"

// Layout, same numbers as the mock: grid at (GX,GY), 11 px cells, panel at PX.
static const int N = GRID_N, CS = 11, GX = 16, GY = 12, PX = 134, PANEL_W = 104;
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

// ---------- text helpers ----------
static void text(M5Canvas& c, const char* s, int x, int y, uint32_t col, uint8_t datum = top_left) {
  c.setFont(&fonts::Font0);  // 8 px tall, the smallest allowed
  c.setTextSize(1);
  c.setTextDatum(datum);
  c.setTextColor(rgb(col));
  c.drawString(s, x, y);
}

// Bold heading; y is the baseline. Shrinks a little if it would not fit in maxW.
static void heading(M5Canvas& c, const char* s, int x, int y, uint32_t col, uint8_t datum = baseline_left,
                    int maxW = PANEL_W) {
  c.setFont(&fonts::FreeSansBold9pt7b);
  c.setTextSize(1);
  int w = c.textWidth(s);
  if (w > maxW) c.setTextSize(maxW / float(w));
  c.setTextDatum(datum);
  c.setTextColor(rgb(col));
  c.drawString(s, x, y);
  c.setTextSize(1);
}

// A little key drawn like a keyboard key, e.g. [tab]. y is its top. Returns its width.
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

// Column letters and row numbers. The cursor's column and row light up (-1 = none).
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

// Pulsing corner brackets around the square under the cursor (used in battle).
__attribute__((unused)) static void drawCursor(M5Canvas& c, int cx, int cy, uint32_t t) {
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

// ---------- ships ----------
enum class Look { Normal, Sunk, GhostOk, GhostBad };

// A hull with a pointed bow, a bridge in the middle and guns.
// The hull is drawn as 9 lines along the ship; "along" runs stern to bow.
// Horizontal ships point right, vertical ships point up (as in the mock).
static void drawShip(M5Canvas& c, const Ship& s, Look look) {
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
  auto span = [&](int a, int u0, int u1, uint16_t col) {
    if (h) c.drawFastHLine(x0 + u0, y0 + 2 + a, u1 - u0 + 1, col);
    else c.drawFastVLine(x0 + 2 + a, y0 + L - 1 - u1, u1 - u0 + 1, col);
  };

  const bool ghost = look == Look::GhostOk || look == Look::GhostBad;
  uint32_t edge;
  if (look == Look::GhostOk) edge = 0x9FF5C0;
  else if (look == Look::GhostBad) edge = 0xFF9A9A;
  else if (look == Look::Sunk) edge = 0x140807;
  else edge = 0x25313D;
  uint32_t ghostFill = mix(seaAt(y0 + 5, false), look == Look::GhostOk ? C_GREEN : 0xF03E3E, 0.45f);

  for (int a = 0; a < 9; a++) {
    span(a, from[a], to[a], rgb(edge));  // whole line in the outline colour...
    if (a == 0 || a == 8) continue;
    // ...then the inside, leaving a 1 px outline also where the neighbouring lines are shorter
    int u0 = max(from[a] + 1, max(from[a - 1], from[a + 1]));
    int u1 = min(to[a] - 1, min(to[a - 1], to[a + 1]));
    if (u0 > u1) continue;
    float k = a / 8.0f;
    uint32_t fill;
    if (ghost) fill = ghostFill;
    else if (look == Look::Sunk) fill = mix(0x6A3128, 0x2A1210, k);
    else fill = k < 0.45f ? mix(0xC9D5DF, 0x8C9DAF, k / 0.45f) : mix(0x8C9DAF, 0x52627A, (k - 0.45f) / 0.55f);
    span(a, u0, u1, rgb(fill));
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

  c.setFont(&fonts::Font2);
  c.setTextDatum(top_left);
  c.setTextColor(rgb(0xFFFFFF));
  c.drawString(type.name, PX, 22);
  miniHull(c, PX, 40, type.size * 10, 0xC9D5DF, 0x52627A);

  for (int i = 0; i < NUM_SHIPS; i++) {  // progress: placed, current, still to come
    uint32_t col = i < g.fleet.count ? C_GREEN : i == g.fleet.count ? C_YELLOW : 0x2C3E48;
    c.fillCircle(PX + 3 + i * 9, 56, 2, rgb(col));
  }

  keyHint(c, "r", "rotate", PX, 68);
  keyHint(c, "a", "auto", PX, 82);
  keyHint(c, "ok", "place", PX, 96);
  if (g.msg[0][0]) text(c, g.msg[0], PX, 119, C_GREEN);
}

static void panelWait(M5Canvas& c, uint32_t t) {
  heading(c, "FLEET READY", PX, 17, C_GREEN);
  text(c, "Waiting for", PX, 30, C_WHITE);
  text(c, "partner", PX, 41, C_WHITE);
  for (int i = 0; i < 3; i++) {
    float a = (sinf(t / 200.0f - i * 0.7f) + 1) / 2;
    c.fillCircle(PX + 46 + i * 6, 47, 1, rgb(mix(0x0F1D25, C_GREEN, 0.3f + 0.7f * a)));
  }
  text(c, "First one ready", PX, 64, C_MUTED);
  text(c, "fires first.", PX, 75, C_MUTED);
}

// Placeholder until milestone 5: shows whose turn it would be.
static void panelBattle(M5Canvas& c, const Game& g) {
  c.fillRoundRect(PX - 1, 6, 101, 17, 4, rgb(g.myTurn ? 0x2B8A3E : 0x2A3A44));
  heading(c, g.myTurn ? "YOUR TURN" : "THEIR TURN", PX + 49, 20, 0xFFFFFF, baseline_center, 96);
  text(c, g.msg[0], PX, 36, C_WHITE);
  text(c, g.msg[1], PX, 47, C_YELLOW);
  text(c, "Firing comes in", PX, 72, C_MUTED);
  text(c, "milestone 5.", PX, 83, C_MUTED);
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

  heading(c, "BATTLESHIP", 114, 52, C_YELLOW, baseline_left, 122);
  text(c, "two-player, ESP-NOW", 115, 60, C_MUTED);
  if (lost) text(c, "partner lost!", 115, 74, 0xFF8787);
  text(c, "searching for", 115, 86, C_WHITE);
  char dots[12];
  snprintf(dots, sizeof dots, "partner%.*s", int(t / 400 % 4), "...");
  text(c, dots, 115, 97, C_WHITE);
  text(c, myName, 115, 118, C_LABEL);
}

// ---------- whole frame ----------
void drawGame(M5Canvas& c, const Game& g, uint32_t t, const char* myName, bool lost) {
  if (g.phase == Phase::Search) {
    drawSearch(c, t, myName, lost);
    return;
  }

  drawBackground(c);
  const bool placing = g.phase == Phase::Place;
  drawLabels(c, placing ? g.cursorX : -1, placing ? g.cursorY : -1);
  drawSea(c, t, false);
  for (int i = 0; i < g.fleet.count; i++) drawShip(c, g.fleet.ships[i], Look::Normal);
  if (placing) {
    Ship gh = g.ghost();
    c.setClipRect(GX, GY, N * CS + 1, N * CS + 1);  // a ghost hanging off the edge gets cut off
    drawShip(c, gh, g.fleet.fits(gh) ? Look::GhostOk : Look::GhostBad);
    c.clearClipRect();
  }

  drawPanel(c);
  if (placing) panelPlace(c, g);
  else if (g.phase == Phase::Waiting) panelWait(c, t);
  else panelBattle(c, g);
}
