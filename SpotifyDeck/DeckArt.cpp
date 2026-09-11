#include "DeckArt.h"

#include <LittleFS.h>
#include <TJpg_Decoder.h>

#include "DeckColor.h"
#include "DeckConfig.h"
#include "DeckSpotify.h"
#include "DeckTft.h"

namespace {

// The thumbnail for the background. Decoded at 1/8 scale, so 300x300
// gives 37x37 and 640x640 (should Spotify send nothing smaller) exactly
// 80x80.
constexpr int THUMB_MAX = 80;

uint16_t g_thumb[THUMB_MAX * THUMB_MAX];
int      g_tw = 0;
int      g_th = 0;

uint8_t* g_jpeg    = nullptr;
size_t   g_jpegLen = 0;
char     g_id[48]  = {0};

// The substitute palette for when we have no artwork.
uint32_t g_dominant = Col::rgb(0x1E, 0xD7, 0x60);
uint32_t g_accent   = Col::rgb(0x1E, 0xD7, 0x60);
uint32_t g_bgTop    = Col::rgb(0x12, 0x1C, 0x30);
uint32_t g_bgBottom = Col::rgb(0x05, 0x07, 0x0C);

// Automatic background "exposure": 255 = unchanged. Derived from the
// average lightness of the artwork, so bright covers (brat) do not wash
// out the white text and dark covers do not go completely black.
uint16_t g_bgGain = 130;

// The palette computed from the album art is kept separately. The other
// pages overwrite the "live" values through setFlatPalette(), and without
// this backup the color derived from the artwork would never come back -
// Art::load() returns immediately when the art has not changed and does
// not recompute it.
uint32_t g_artDominant = 0;
uint32_t g_artAccent   = 0;
uint32_t g_artBgTop    = 0;
uint32_t g_artBgBottom = 0;
uint16_t g_artBgGain   = 130;
bool     g_artPalette  = false;

bool g_fsReady  = false;
bool g_useArt   = true;    // false = a smooth gradient instead of blurred art

constexpr uint8_t  BG_TARGET_LUMA = 52;   // target mean lightness of the background
constexpr uint16_t BG_VIGNETTE    = 92;   // how much to darken towards the bottom

// --- the background row is computed pixel by pixel, this is the buffer -
uint16_t g_rowBuf[SCREEN_W];

// A 4x4 dither matrix (Bayer). RGB565 has only 64 levels in the green
// channel, so a gradient spanning the whole height of the display falls
// apart into visible bands. Adding this "noise" correction before the
// rounding breaks the bands up and it reads as a smooth gradient.
const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

inline uint16_t dither565(uint32_t c, int x, int y) {
  int t = BAYER[((y & 3) << 2) | (x & 3)];      // 0..15
  int r = (int)Col::R(c) + (t >> 1) - 4;        // R/B: 3 bits get thrown away
  int g = (int)Col::G(c) + (t >> 2) - 2;        // G:   2 bits
  int b = (int)Col::B(c) + (t >> 1) - 4;
  return Col::to565((uint8_t)constrain(r, 0, 255),
                    (uint8_t)constrain(g, 0, 255),
                    (uint8_t)constrain(b, 0, 255));
}

// -------------------------------------------------------------------
//  Decoder callbacks
// -------------------------------------------------------------------
bool cbThumb(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
  for (uint16_t j = 0; j < h; j++) {
    int ty = y + j;
    if (ty < 0 || ty >= g_th) continue;
    for (uint16_t i = 0; i < w; i++) {
      int tx = x + i;
      if (tx < 0 || tx >= g_tw) continue;
      g_thumb[ty * g_tw + tx] = bmp[j * w + i];
    }
  }
  return true;
}

bool cbTft(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
  tft.pushImage(x, y, w, h, bmp);   // pushImage clips itself against the viewport
  return true;
}

// -------------------------------------------------------------------
//  Thumbnail processing
// -------------------------------------------------------------------
void boxBlur() {
  if (g_tw < 3 || g_th < 3) return;

  uint16_t prev[THUMB_MAX], cur[THUMB_MAX], out[THUMB_MAX], nxt[THUMB_MAX];

  memcpy(cur, &g_thumb[0], g_tw * sizeof(uint16_t));
  memcpy(prev, cur, g_tw * sizeof(uint16_t));

  for (int y = 0; y < g_th; y++) {
    if (y + 1 < g_th) memcpy(nxt, &g_thumb[(y + 1) * g_tw], g_tw * sizeof(uint16_t));
    else              memcpy(nxt, cur, g_tw * sizeof(uint16_t));

    for (int x = 0; x < g_tw; x++) {
      uint16_t r = 0, g = 0, b = 0;
      for (int dx = -1; dx <= 1; dx++) {
        int sx = constrain(x + dx, 0, g_tw - 1);
        const uint16_t src[3] = {prev[sx], cur[sx], nxt[sx]};
        for (int k = 0; k < 3; k++) {
          uint32_t c = Col::to888(src[k]);
          r += Col::R(c); g += Col::G(c); b += Col::B(c);
        }
      }
      out[x] = Col::to565((uint8_t)(r / 9), (uint8_t)(g / 9), (uint8_t)(b / 9));
    }

    memcpy(prev, cur, g_tw * sizeof(uint16_t));
    memcpy(cur,  nxt, g_tw * sizeof(uint16_t));
    memcpy(&g_thumb[y * g_tw], out, g_tw * sizeof(uint16_t));
  }
}

// Dominant color: the average over the strongest hue bucket. A plain
// average of every pixel would come out as mud on most covers.
void computePalette() {
  constexpr int BUCKETS = 16;
  uint32_t sumR[BUCKETS] = {0}, sumG[BUCKETS] = {0}, sumB[BUCKETS] = {0};
  uint32_t cnt[BUCKETS]  = {0};
  uint32_t grayR = 0, grayG = 0, grayB = 0, grayN = 0;
  uint32_t lumaSum = 0;
  int total = g_tw * g_th;
  if (total <= 0) return;

  for (int i = 0; i < total; i++) {
    uint32_t c = Col::to888(g_thumb[i]);
    lumaSum += Col::luma(c);

    Col::Hsv h = Col::toHsv(c);
    if (h.v < 30 || h.s < 60) {
      grayR += Col::R(c); grayG += Col::G(c); grayB += Col::B(c); grayN++;
      continue;
    }
    int b = (h.h * BUCKETS) / 360;
    if (b >= BUCKETS) b = BUCKETS - 1;
    sumR[b] += Col::R(c); sumG[b] += Col::G(c); sumB[b] += Col::B(c);
    cnt[b]++;
  }

  int best = -1;
  uint32_t bestN = 0;
  for (int b = 0; b < BUCKETS; b++) {
    if (cnt[b] > bestN) { bestN = cnt[b]; best = b; }
  }

  // A color bucket only wins if it is not completely marginal.
  if (best >= 0 && bestN * 8 >= (uint32_t)total) {
    g_dominant = Col::rgb((uint8_t)(sumR[best] / bestN),
                          (uint8_t)(sumG[best] / bestN),
                          (uint8_t)(sumB[best] / bestN));
  } else if (grayN) {
    g_dominant = Col::rgb((uint8_t)(grayR / grayN),
                          (uint8_t)(grayG / grayN),
                          (uint8_t)(grayB / grayN));
  }

  g_accent   = Col::accentFrom(g_dominant);
  g_bgTop    = Col::shadeFrom(g_dominant, 62);
  g_bgBottom = Col::shadeFrom(g_dominant, 18);

  uint8_t avgLuma = (uint8_t)(lumaSum / total);
  if (avgLuma < 1) avgLuma = 1;
  int gain = 255 * BG_TARGET_LUMA / avgLuma;
  g_bgGain = (uint16_t)constrain(gain, 45, 200);

  g_artDominant = g_dominant;
  g_artAccent   = g_accent;
  g_artBgTop    = g_bgTop;
  g_artBgBottom = g_bgBottom;
  g_artBgGain   = g_bgGain;
  g_artPalette  = true;

  LOGF("[art] dominant=%06X accent=%06X avgLuma=%u gain=%u\n",
       (unsigned)g_dominant, (unsigned)g_accent, avgLuma, g_bgGain);
}

bool decodeThumb() {
  uint16_t jw = 0, jh = 0;
  if (TJpgDec.getJpgSize(&jw, &jh, g_jpeg, g_jpegLen) != JDR_OK) return false;
  if (!jw || !jh) return false;

  // 1/8 is the smallest scale TJpgDec can do - exactly what we want.
  // 300x300 -> 37x37, 640x640 -> 80x80, both fit inside THUMB_MAX.
  const uint8_t scale = 8;
  g_tw = min<int>(jw / scale, THUMB_MAX);
  g_th = min<int>(jh / scale, THUMB_MAX);
  if (g_tw < 2 || g_th < 2) return false;

  memset(g_thumb, 0, sizeof(uint16_t) * g_tw * g_th);

  TJpgDec.setJpgScale(scale);
  TJpgDec.setSwapBytes(false);       // we want native RGB565 for the math
  TJpgDec.setCallback(cbThumb);
  JRESULT r = TJpgDec.drawJpg(0, 0, g_jpeg, g_jpegLen);
  if (r != JDR_OK && r != JDR_INTR) {
    LOGF("[art] thumb decode failed (%d)\n", (int)r);
    return false;
  }

  boxBlur();
  boxBlur();
  computePalette();
  return true;
}

// -------------------------------------------------------------------
//  Background sampling
// -------------------------------------------------------------------
struct RowMap {
  int32_t sxStep;   // 16.16
  int32_t sx0;
  int32_t sy;       // 16.16
  uint16_t fade;    // 0..255, brightness multiplier for this row
};

RowMap mapRow(int16_t y) {
  RowMap m;
  // A "cover" fit: the thumbnail is stretched to the full display width
  // and cropped symmetrically in height.
  int visH = (int)((int32_t)g_th * SCREEN_H / SCREEN_W);
  if (visH > g_th) visH = g_th;
  int topOff = (g_th - visH) / 2;

  m.sxStep = ((int32_t)(g_tw - 1) << 16) / (SCREEN_W - 1);
  m.sx0    = 0;
  m.sy     = ((int32_t)topOff << 16) +
             (int32_t)(((int64_t)(visH - 1) * y << 16) / (SCREEN_H - 1));

  int fade = (int)g_bgGain - (int)((int32_t)BG_VIGNETTE * y / SCREEN_H);
  m.fade = (uint16_t)constrain(fade, 20, 255);
  return m;
}

inline uint32_t sampleThumb(int32_t sx, int32_t sy) {
  int x0 = sx >> 16, y0 = sy >> 16;
  int x1 = min(x0 + 1, g_tw - 1);
  int y1 = min(y0 + 1, g_th - 1);
  x0 = constrain(x0, 0, g_tw - 1);
  y0 = constrain(y0, 0, g_th - 1);

  uint16_t fx = (uint16_t)((sx >> 8) & 0xFF);
  uint16_t fy = (uint16_t)((sy >> 8) & 0xFF);

  uint32_t c00 = Col::to888(g_thumb[y0 * g_tw + x0]);
  uint32_t c10 = Col::to888(g_thumb[y0 * g_tw + x1]);
  uint32_t c01 = Col::to888(g_thumb[y1 * g_tw + x0]);
  uint32_t c11 = Col::to888(g_thumb[y1 * g_tw + x1]);

  uint32_t top = Col::mix(c00, c10, (uint8_t)fx);
  uint32_t bot = Col::mix(c01, c11, (uint8_t)fx);
  return Col::mix(top, bot, (uint8_t)fy);
}

bool blurUsable() {
#if FEAT_BLUR_BACKGROUND
  return g_useArt && g_tw >= 2 && g_th >= 2;
#else
  return false;
#endif
}

// -------------------------------------------------------------------
//  Artwork cache in LittleFS
// -------------------------------------------------------------------
#if FEAT_ART_CACHE

void cachePath(const char* id, char* out, size_t cap) {
  snprintf(out, cap, "/art/%s.jpg", id);
}

size_t cacheRead(const char* id, uint8_t** buf) {
  if (!g_fsReady) return 0;
  char path[80];
  cachePath(id, path, sizeof(path));
  if (!LittleFS.exists(path)) return 0;

  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  size_t len = f.size();
  if (len == 0 || len > ART_MAX_BYTES) { f.close(); return 0; }

  uint8_t* p = (uint8_t*)malloc(len);
  if (!p) { f.close(); return 0; }
  size_t got = f.read(p, len);
  f.close();
  if (got != len) { free(p); return 0; }

  *buf = p;
  LOGF("[art] cache hit %s (%u B)\n", id, (unsigned)len);
  return len;
}

// The simplest possible "LRU": when there are too many files, delete
// the oldest one.
void cachePrune(size_t maxFiles) {
  if (!g_fsReady) return;
  File dir = LittleFS.open("/art");
  if (!dir || !dir.isDirectory()) return;

  size_t count = 0;
  char oldest[80] = {0};
  time_t oldestTime = 0;

  File f = dir.openNextFile();
  while (f) {
    count++;
    time_t t = f.getLastWrite();
    if (oldest[0] == '\0' || t < oldestTime) {
      oldestTime = t;
      snprintf(oldest, sizeof(oldest), "/art/%s", f.name());
    }
    f.close();
    f = dir.openNextFile();
  }
  dir.close();

  if (count > maxFiles && oldest[0]) {
    LOGF("[art] cache prune %s\n", oldest);
    LittleFS.remove(oldest);
  }
}

void cacheWrite(const char* id, const uint8_t* buf, size_t len) {
  if (!g_fsReady || !len) return;
  cachePrune(10);

  char path[80];
  cachePath(id, path, sizeof(path));
  File f = LittleFS.open(path, "w");
  if (!f) return;
  f.write(buf, len);
  f.close();
  LOGF("[art] cache write %s (%u B)\n", id, (unsigned)len);
}

#endif  // FEAT_ART_CACHE

}  // namespace

// =====================================================================
//  Public API
// =====================================================================
void Art::begin() {
#if FEAT_ART_CACHE
  g_fsReady = LittleFS.begin(true);      // true = format on the first boot
  if (g_fsReady) {
    if (!LittleFS.exists("/art")) LittleFS.mkdir("/art");
    LOGF("[art] LittleFS ok, %u B free\n",
         (unsigned)(LittleFS.totalBytes() - LittleFS.usedBytes()));
  } else {
    LOGLN("[art] LittleFS would not mount - cache disabled");
  }
#endif
}

void Art::unload() {
  if (g_jpeg) { free(g_jpeg); g_jpeg = nullptr; }
  g_jpegLen  = 0;
  g_id[0]    = '\0';
  g_tw = g_th = 0;

  g_dominant = Col::rgb(0x1E, 0xD7, 0x60);
  g_accent   = Col::rgb(0x1E, 0xD7, 0x60);
  g_bgTop    = Col::rgb(0x12, 0x1C, 0x30);
  g_bgBottom = Col::rgb(0x05, 0x07, 0x0C);
  g_bgGain   = 130;
  g_artPalette = false;
}

bool Art::load(const char* artId, const char* url) {
  if (!artId || !*artId || !url || !*url) return false;
  if (g_jpeg && strcmp(g_id, artId) == 0) return true;   // we already have it

  // Free the old art BEFORE downloading, or two of them sit in memory.
  if (g_jpeg) { free(g_jpeg); g_jpeg = nullptr; g_jpegLen = 0; }

  uint8_t* buf = nullptr;
  size_t   len = 0;

#if FEAT_ART_CACHE
  len = cacheRead(artId, &buf);
#endif

  if (!len) {
    len = Spotify::fetchArtwork(url, &buf);
#if FEAT_ART_CACHE
    if (len) cacheWrite(artId, buf, len);
#endif
  }

  if (!len || !buf) { Art::unload(); return false; }

  g_jpeg    = buf;
  g_jpegLen = len;
  strncpy(g_id, artId, sizeof(g_id) - 1);
  g_id[sizeof(g_id) - 1] = '\0';

  if (!decodeThumb()) {
    // Without a thumbnail there is neither a palette nor a blurred
    // background. Drop the id as well so the next attempt retries -
    // otherwise the shortcut on a matching id would freeze this failure
    // in place for that album.
    LOGLN("[art] thumbnail failed, dropping the artwork");
    g_id[0] = '\0';
    g_tw = g_th = 0;
    return false;
  }
  return true;
}

void Art::useArtBackground(bool on) {
  g_useArt = on;
  // Coming back to the Spotify page has to restore the colors from the
  // artwork too, not just switch the background source.
  if (on && g_artPalette) {
    g_dominant = g_artDominant;
    g_accent   = g_artAccent;
    g_bgTop    = g_artBgTop;
    g_bgBottom = g_artBgBottom;
    g_bgGain   = g_artBgGain;
  }
}

void Art::setFlatPalette(uint32_t top, uint32_t bottom, uint32_t accent) {
  g_bgTop    = top;
  g_bgBottom = bottom;
  g_accent   = accent;
  g_dominant = accent;
}

bool        Art::hasArt()    { return g_useArt && g_jpeg != nullptr && g_jpegLen > 0; }
const char* Art::currentId() { return g_id; }
uint32_t    Art::dominant()  { return g_dominant; }
uint32_t    Art::accent()    { return g_accent; }

// ---------------------------------------------------------------------
//  Background
// ---------------------------------------------------------------------
uint16_t Art::pixelAt(int16_t x, int16_t y) {
  x = constrain(x, (int16_t)0, (int16_t)(SCREEN_W - 1));
  y = constrain(y, (int16_t)0, (int16_t)(SCREEN_H - 1));

  if (!blurUsable()) {
    uint8_t t = (uint8_t)((int32_t)y * 255 / (SCREEN_H - 1));
    return dither565(Col::mix(g_bgTop, g_bgBottom, t), x, y);
  }

  RowMap m = mapRow(y);
  uint32_t c = sampleThumb(m.sx0 + (int32_t)x * m.sxStep, m.sy);
  c = Col::scale(c, m.fade);
  return dither565(Col::mix(c, g_bgBottom, 46), x, y);
}

void Art::rowInto(uint16_t* dst, int16_t x, int16_t y, int16_t w) {
  if (w <= 0) return;
  y = constrain(y, (int16_t)0, (int16_t)(SCREEN_H - 1));

  if (!blurUsable()) {
    uint8_t t = (uint8_t)((int32_t)y * 255 / (SCREEN_H - 1));
    uint32_t c = Col::mix(g_bgTop, g_bgBottom, t);
    for (int16_t i = 0; i < w; i++) dst[i] = dither565(c, x + i, y);
    return;
  }

  RowMap m = mapRow(y);
  int32_t sx = m.sx0 + (int32_t)x * m.sxStep;
  for (int16_t i = 0; i < w; i++) {
    int32_t cx = constrain(sx, (int32_t)0, (int32_t)(g_tw - 1) << 16);
    uint32_t c = sampleThumb(cx, m.sy);
    c = Col::scale(c, m.fade);
    dst[i] = dither565(Col::mix(c, g_bgBottom, 46), x + i, y);
    sx += m.sxStep;
  }
}

void Art::paintRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0) return;

  bool swap = tft.getSwapBytes();
  tft.setSwapBytes(true);
  for (int16_t row = 0; row < h; row++) {
    Art::rowInto(g_rowBuf, x, y + row, w);
    tft.pushImage(x, y + row, w, 1, g_rowBuf);
  }
  tft.setSwapBytes(swap);
}

void Art::paintBackground() {
  Art::paintRect(0, 0, SCREEN_W, SCREEN_H);
}

// ---------------------------------------------------------------------
//  Cover
// ---------------------------------------------------------------------
namespace {

// After the square is drawn, put the corner pixels back to the background
// so the cover reads as a rounded rectangle and not a hard square.
void roundCorners(int16_t x, int16_t y, int16_t size, int16_t r) {
  if (r <= 0) return;
  int32_t rr = (int32_t)r * r;
  bool swap = tft.getSwapBytes();
  tft.setSwapBytes(true);

  for (int16_t dy = 0; dy < r; dy++) {
    for (int16_t dx = 0; dx < r; dx++) {
      int32_t ex = r - 1 - dx;
      int32_t ey = r - 1 - dy;
      if (ex * ex + ey * ey <= rr) continue;

      const int16_t px[4] = {(int16_t)(x + dx), (int16_t)(x + size - 1 - dx),
                             (int16_t)(x + dx), (int16_t)(x + size - 1 - dx)};
      const int16_t py[4] = {(int16_t)(y + dy), (int16_t)(y + dy),
                             (int16_t)(y + size - 1 - dy),
                             (int16_t)(y + size - 1 - dy)};
      for (int k = 0; k < 4; k++) {
        uint16_t c = Art::pixelAt(px[k], py[k]);
        tft.drawPixel(px[k], py[k], c);
      }
    }
  }
  tft.setSwapBytes(swap);
}

// Picks the largest decoder scale that still fills the requested area.
uint8_t scaleFor(uint16_t srcW, int16_t want) {
  uint8_t scale = 1;
  while (scale < 8 && (srcW / (uint16_t)(scale * 2)) >= (uint16_t)want) scale *= 2;
  return scale;
}

}  // namespace

bool Art::drawCover(int16_t x, int16_t y, int16_t size) {
  if (!hasArt()) return false;

  uint16_t jw = 0, jh = 0;
  if (TJpgDec.getJpgSize(&jw, &jh, g_jpeg, g_jpegLen) != JDR_OK || !jw) return false;

  uint8_t scale = scaleFor(jw, size);
  int dw = jw / scale, dh = jh / scale;

  bool swap = tft.getSwapBytes();
  tft.setSwapBytes(true);
  tft.setViewport(x, y, size, size);

  TJpgDec.setJpgScale(scale);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(cbTft);
  JRESULT r = TJpgDec.drawJpg((size - dw) / 2, (size - dh) / 2, g_jpeg, g_jpegLen);

  tft.resetViewport();
  tft.setSwapBytes(swap);

  if (r != JDR_OK && r != JDR_INTR) {
    LOGF("[art] cover decode failed (%d)\n", (int)r);
    return false;
  }

  roundCorners(x, y, size, ART_RADIUS);

  // A faint light outline so the cover does not blend into the blurred
  // background. The color comes from the background around the cover, so
  // it fits whatever happens to be behind it.
  uint32_t around = Col::to888(Art::pixelAt(x + size / 2, y - 2));
  around = Col::mix(around, Col::to888(Art::pixelAt(x + size / 2, y + size + 1)), 128);
  tft.drawRoundRect(x - 1, y - 1, size + 2, size + 2, ART_RADIUS + 1,
                    Col::to565(Col::mix(around, Col::rgb(255, 255, 255), 90)));
  return true;
}

bool Art::drawCoverFull() {
  if (!hasArt()) return false;

  uint16_t jw = 0, jh = 0;
  if (TJpgDec.getJpgSize(&jw, &jh, g_jpeg, g_jpegLen) != JDR_OK || !jw) return false;

  // We want to fill the height; the excess gets cropped.
  uint8_t scale = scaleFor(jw, SCREEN_H);
  int dw = jw / scale, dh = jh / scale;

  bool swap = tft.getSwapBytes();
  tft.setSwapBytes(true);
  tft.setViewport(0, 0, SCREEN_W, SCREEN_H);

  TJpgDec.setJpgScale(scale);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(cbTft);
  JRESULT r = TJpgDec.drawJpg((SCREEN_W - dw) / 2, (SCREEN_H - dh) / 2,
                              g_jpeg, g_jpegLen);

  tft.resetViewport();
  tft.setSwapBytes(swap);
  return (r == JDR_OK || r == JDR_INTR);
}
