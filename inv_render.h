// inv_render.h - software renderer for the 3D space invaders (pure C++, no Arduino dependencies)
//
// The ground and the sky are ray-cast per row (the camera has no roll, so the horizon is always a horizontal
// line and everything about a row is computed once). The invaders, bolts and explosions are pixel-art sprites
// drawn as billboards, scaled by perspective. Everything is plain 70s pixel art, just in smooth 3D.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "inv_sprites.h"
#include "inv_game.h"

#pragma GCC push_options
#pragma GCC optimize ("O3")

#define IW 240
#define IC565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

static float inv_star[70][4];                 // direction x, y, z and a twinkle phase
static int   inv_nstars = 0;

static inline void ip(uint16_t *fb, int x, int y, uint16_t c) { if ((unsigned)x < (unsigned)IW && (unsigned)y < (unsigned)IW) fb[y * IW + x] = c; }
static inline uint32_t inv_hash(int a, int b) {
  uint32_t h = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h;
}
static inline uint16_t inv_mix565(uint16_t a, int r, int g, int b, int w32) {          // blend a pixel toward (r,g,b), w32 = 0..32
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  ar = (ar << 3) | (ar >> 2); ag = (ag << 2) | (ag >> 4); ab = (ab << 3) | (ab >> 2);
  ar += ((r - ar) * w32) >> 5; ag += ((g - ag) * w32) >> 5; ab += ((b - ab) * w32) >> 5;
  return IC565(ar, ag, ab);
}

static void inv_render_init(uint32_t seed) {
  uint32_t s = seed ? seed : 99u;
  inv_nstars = 0;
  while (inv_nstars < 70) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; float az = (float)(s & 0xFFFF) / 65536.0f * 6.2831853f;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; float se = 0.05f + 0.94f * (float)(s & 0xFFFF) / 65536.0f;   // sin(elevation)
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; float ph = (float)(s & 0xFFFF) / 65536.0f * 6.28f;
    float ce = sqrtf(1.0f - se * se);
    inv_star[inv_nstars][0] = sinf(az) * ce; inv_star[inv_nstars][1] = se; inv_star[inv_nstars][2] = cosf(az) * ce; inv_star[inv_nstars][3] = ph;
    inv_nstars++;
  }
}

// ---------------- sky and ground colours ----------------
static inline void inv_sky_rgb(float sinEl, int &r, int &g, int &b) {          // flat bands, 70s style
  if (sinEl < 0.052f)      { r = 255; g = 168; b = 70; }
  else if (sinEl < 0.139f) { r = 238; g = 112; b = 76; }
  else if (sinEl < 0.276f) { r = 176; g = 62;  b = 104; }
  else if (sinEl < 0.5f)   { r = 94;  g = 36;  b = 104; }
  else if (sinEl < 0.766f) { r = 44;  g = 22;  b = 76; }
  else                     { r = 14;  g = 9;   b = 36; }
}

// ---------------- sprites ----------------
static void inv_sprite(uint16_t *fb, const uint16_t *rows, int w, int h, float cx, float cy, float scale, uint16_t col, int dropSeed = 0) {
  if (scale < 0.15f) return;
  float ox = cx - 0.5f * (float)w * scale, oy = cy - 0.5f * (float)h * scale;
  int x0 = (int)floorf(ox), x1 = (int)ceilf(ox + (float)w * scale), y0 = (int)floorf(oy), y1 = (int)ceilf(oy + (float)h * scale);
  if (x1 < 0 || y1 < 0 || x0 >= IW || y0 >= IW) return;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > IW) x1 = IW;
  if (y1 > IW) y1 = IW;
  float inv = 1.0f / scale;
  for (int py = y0; py < y1; py++) {
    int r = (int)floorf(((float)py + 0.5f - oy) * inv);
    if (r < 0 || r >= h) continue;
    uint16_t bits = rows[r];
    if (!bits) continue;
    uint16_t *row = fb + py * IW;
    for (int px = x0; px < x1; px++) {
      int c = (int)floorf(((float)px + 0.5f - ox) * inv);
      if (c < 0 || c >= w) continue;
      if (!((bits >> (w - 1 - c)) & 1)) continue;
      if (dropSeed && (inv_hash(r * 31 + dropSeed, c) % 5) == 0) continue;           // flicker for explosions
      row[px] = col;
    }
  }
}

// ---------------- text (3x5 font) ----------------
static void inv_text(uint16_t *fb, const char *s, int cx, int y, int scale, uint16_t col) {
  int n = (int)strlen(s);
  int total = n * 4 * scale - scale;
  int x = cx - total / 2;
  for (int k = 0; k < n; k++, x += 4 * scale) {
    const char *p = strchr(INV_FONT_CH, s[k]);
    if (!p) continue;
    const uint8_t *g = INV_FONT_ROWS[p - INV_FONT_CH];
    for (int pass = 0; pass < 2; pass++)
      for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++) {
          if (!((g[r] >> (2 - c)) & 1)) continue;
          int dx = pass == 0 ? 1 : 0;                                                 // drop shadow first
          uint16_t cc = pass == 0 ? IC565(10, 10, 14) : col;
          for (int j = 0; j < scale; j++) for (int i = 0; i < scale; i++) ip(fb, x + c * scale + i + dx, y + r * scale + j + dx, cc);
        }
  }
}

static const uint16_t INV_GREEN = IC565(80, 255, 110);

// ---------------- the whole frame ----------------
static void inv_render(uint16_t *fb, float yaw, float pitch) {
  const float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
  const float fx = sy * cp, fy = sp, fz = cy * cp;                       // forward
  const float rx = cy, rz = -sy;                                          // right (no vertical part)
  const float ux = -sy * sp, uy = cp, uz = -cy * sp;                      // up
  const float invF = 1.0f / INV_F;

  // ---- sky and ground, row by row
  const int toneRGB[3][3] = {{224, 172, 98}, {204, 148, 82}, {176, 122, 66}};
  const int hazeR = 255, hazeG = 168, hazeB = 70;
  for (int y = 0; y < IW; y++) {
    int dyc = y - 120;
    int hw = (int)sqrtf(fmaxf(0.0f, 121.0f * 121.0f - (float)(dyc * dyc)));
    int xa = 120 - hw, xb = 120 + hw; if (xa < 0) xa = 0; if (xb > IW - 1) xb = IW - 1;
    uint16_t *row = fb + y * IW;
    float v = (120.0f - ((float)y + 0.5f)) * invF;
    float dy = v * uy + fy;
    if (dy >= -1e-4f) {
      int r, g, b; inv_sky_rgb(dy / sqrtf(1.0f + v * v), r, g, b);
      uint16_t c = IC565(r, g, b);
      for (int x = xa; x <= xb; x++) row[x] = c;
      continue;
    }
    float t = INV_EYE_H / -dy;                                            // distance along the ray to the ground
    float fog = (t - 18.0f) * (1.0f / 70.0f);
    if (fog >= 1.0f) { uint16_t c = IC565(hazeR, hazeG, hazeB); for (int x = xa; x <= xb; x++) row[x] = c; continue; }
    if (fog < 0) fog = 0;
    uint16_t tone[3], spD, spL;
    for (int k = 0; k < 3; k++) tone[k] = IC565((int)(toneRGB[k][0] + (hazeR - toneRGB[k][0]) * fog), (int)(toneRGB[k][1] + (hazeG - toneRGB[k][1]) * fog), (int)(toneRGB[k][2] + (hazeB - toneRGB[k][2]) * fog));
    spD = IC565((int)(138 + (hazeR - 138) * fog), (int)(92 + (hazeG - 92) * fog), (int)(50 + (hazeB - 50) * fog));
    spL = IC565((int)(244 + (hazeR - 244) * fog), (int)(200 + (hazeG - 200) * fog), (int)(124 + (hazeB - 124) * fog));
    float px0 = t * (v * ux + fx), pz0 = t * (v * uz + fz);
    float dpx = t * rx * invF, dpz = t * rz * invF;
    bool fine = t < 14.0f;
    for (int x = xa; x <= xb; x++) {
      float u = (float)x - 119.5f;
      float Px = px0 + u * dpx, Pz = pz0 + u * dpz;
      uint32_t h = inv_hash((int)floorf(Px * (1.0f / 6.0f)), (int)floorf(Pz * (1.0f / 6.0f)));      // dunes: patches of 6 m
      uint16_t c = tone[h % 3];
      if (fine) {                                                                                 // sand speckle close to the player
        uint32_t f = inv_hash((int)floorf(Px * 3.5f) + 1000, (int)floorf(Pz * 3.5f));
        uint32_t m = f & 15;
        if (m == 0) c = spD; else if (m == 1) c = spL;
      }
      row[x] = c;
    }
  }

  // ---- stars and a moon (always above the horizon)
  for (int i = 0; i < inv_nstars; i++) {
    float sx = inv_star[i][0], syv = inv_star[i][1], sz = inv_star[i][2];
    float zc = sx * fx + syv * fy + sz * fz;
    if (zc < 0.2f) continue;
    float xc = sx * rx + sz * rz, yc = sx * ux + syv * uy + sz * uz;
    int px = (int)(120.0f + INV_F * xc / zc), py = (int)(120.0f - INV_F * yc / zc);
    float tw = sinf(inv_time * 2.0f + inv_star[i][3]);
    uint16_t c = tw > 0.6f ? IC565(255, 255, 255) : (tw > -0.4f ? IC565(220, 220, 255) : IC565(150, 150, 200));
    ip(fb, px, py, c);
    if (i % 7 == 0) { ip(fb, px + 1, py, c); ip(fb, px, py + 1, c); }
  }
  {
    float maz = -0.55f, mel = 0.5f;
    float mx = sinf(maz) * cosf(mel), my = sinf(mel), mz = cosf(maz) * cosf(mel);
    float zc = mx * fx + my * fy + mz * fz;
    if (zc > 0.2f) {
      float xc = mx * rx + mz * rz, yc = mx * ux + my * uy + mz * uz;
      float cxm = 120.0f + INV_F * xc / zc, cym = 120.0f - INV_F * yc / zc, rad = INV_F * 0.096f / zc;
      int x0 = (int)(cxm - rad) - 1, x1 = (int)(cxm + rad) + 1, y0 = (int)(cym - rad) - 1, y1 = (int)(cym + rad) + 1;
      for (int py = y0; py <= y1; py++)
        for (int px = x0; px <= x1; px++) {
          float ddx = (float)px + 0.5f - cxm, ddy = (float)py + 0.5f - cym;
          if (ddx * ddx + ddy * ddy > rad * rad) continue;
          uint32_t h = inv_hash((int)floorf(ddx * 0.28f) + 50, (int)floorf(ddy * 0.28f));
          ip(fb, px, py, (h & 7) == 0 ? IC565(205, 190, 140) : IC565(250, 238, 196));
        }
    }
  }

  // ---- invaders, far to near
  int vis[INV_MAXA]; float vz[INV_MAXA]; int nv = 0;
  float scr[INV_MAXA][3];                                                 // screen x, y and scale of each invader
  for (int i = 0; i < inv_na; i++) {
    if (inv_a[i].hp == 0 && inv_a[i].expl <= 0) continue;
    float px = inv_a[i].x, py = inv_a[i].y - INV_EYE_H, pz = inv_a[i].z;
    float zc = px * fx + py * fy + pz * fz;
    if (zc < 1.0f) continue;
    float xc = px * rx + pz * rz, yc = px * ux + py * uy + pz * uz;
    scr[i][0] = 120.0f + INV_F * xc / zc; scr[i][1] = 120.0f - INV_F * yc / zc; scr[i][2] = INV_F * INV_PIX / zc;
    int k = nv++;
    while (k > 0 && vz[k - 1] < zc) { vz[k] = vz[k - 1]; vis[k] = vis[k - 1]; k--; }
    vz[k] = zc; vis[k] = i;
  }
  const int frame = ((int)(inv_time * 2.0f)) & 1;
  for (int n = 0; n < nv; n++) {
    int i = vis[n];
    if (inv_a[i].hp == 0) {                                               // explosion
      int seed = (int)(inv_time * 20.0f);
      inv_sprite(fb, INV_EXPL, INV_EXPL_W, INV_EXPL_H, scr[i][0], scr[i][1], scr[i][2] * 1.3f, IC565(255, 236, 120), seed);
      continue;
    }
    uint16_t col;
    int t = inv_a[i].type;
    if (inv_a[i].flash > 0 && (((int)(inv_time * 24.0f)) & 1)) col = IC565(255, 255, 255);
    else if (inv_a[i].hp == 1) col = IC565(255, 96, 64);                  // damaged
    else col = t == 0 ? IC565(140, 255, 230) : (t == 1 ? IC565(90, 255, 100) : IC565(210, 255, 110));
    inv_sprite(fb, INV_ALIEN[t][frame], INV_ALIEN_W[t], 8, scr[i][0], scr[i][1], scr[i][2], col);
  }

  // ---- alien bolts (they fly straight at us, so they grow where the alien was)
  for (int b = 0; b < INV_MAXB; b++) {
    if (!inv_b[b].on) continue;
    float s = inv_b[b].t / inv_b[b].T;
    if (s > 0.94f) continue;
    float k = 1.0f - s;
    float px = inv_b[b].ox * k, py = (inv_b[b].oy - INV_EYE_H) * k, pz = inv_b[b].oz * k;
    float zc = px * fx + py * fy + pz * fz;
    if (zc < 1.0f) continue;
    float xc = px * rx + pz * rz, yc = px * ux + py * uy + pz * uz;
    float sx = 120.0f + INV_F * xc / zc, sy2 = 120.0f - INV_F * yc / zc, sc = INV_F * 0.55f / zc;
    int fr = ((int)(inv_time * 12.0f)) & 1;
    inv_sprite(fb, INV_BOLT[fr], INV_BOLT_W, INV_BOLT_H, sx, sy2, sc, fr ? IC565(255, 90, 60) : IC565(255, 230, 80));
  }

  // ---- laser and spark
  if (inv_laserT > 0) {
    int flick = ((int)(inv_time * 60.0f)) & 1;
    uint16_t c = flick ? IC565(210, 255, 255) : IC565(120, 255, 255);
    for (int y = 120; y < 208; y++) { ip(fb, 119, y, c); ip(fb, 120, y, IC565(255, 255, 255)); ip(fb, 121, y, c); }
  }
  if (inv_spark > 0) {
    uint16_t c = IC565(255, 255, 200);
    for (int d = -6; d <= 6; d++) { ip(fb, 120 + d, 120, c); ip(fb, 120, 120 + d, c); if (d % 2 == 0) { ip(fb, 120 + d / 2, 120 + d / 2, c); ip(fb, 120 + d / 2, 120 - d / 2, c); } }
  }

  // ---- the cannon (fixed at the bottom) and the crosshair
  inv_sprite(fb, INV_CANNON, INV_CANNON_W, INV_CANNON_H, 120.0f, 214.0f, 2.0f, IC565(90, 255, 110));
  {
    const int R = 13;
    for (int a = 0; a < 72; a++) {                                        // pixel circle
      float ang = (float)a * 0.0872665f;
      ip(fb, 120 + (int)lroundf(cosf(ang) * R - 0.5f), 120 + (int)lroundf(sinf(ang) * R - 0.5f), INV_GREEN);
    }
    for (int d = R + 3; d <= R + 7; d++) { ip(fb, 119, 120 - d, INV_GREEN); ip(fb, 119, 119 + d, INV_GREEN); ip(fb, 120 - d, 119, INV_GREEN); ip(fb, 119 + d, 119, INV_GREEN); }
    ip(fb, 119, 119, INV_GREEN); ip(fb, 120, 119, INV_GREEN); ip(fb, 119, 120, INV_GREEN); ip(fb, 120, 120, INV_GREEN);
  }

  // ---- HUD: level at the top, remaining bonuses (little cannons) at the bottom
  {
    char buf[8]; int lv = inv_level; int n = 0;
    buf[n++] = 'L';
    if (lv >= 100) buf[n++] = '0' + (lv / 100) % 10;
    if (lv >= 10) buf[n++] = '0' + (lv / 10) % 10;
    buf[n++] = '0' + lv % 10; buf[n] = 0;
    inv_text(fb, buf, 120, 9, 2, INV_GREEN);
    int total = inv_bonus * (INV_ICON_W + 2) - 2, x0 = 120 - total / 2;
    for (int k = 0; k < inv_bonus; k++) inv_sprite(fb, INV_ICON, INV_ICON_W, INV_ICON_H, (float)(x0 + k * (INV_ICON_W + 2)) + 0.5f * INV_ICON_W, 229.0f, 1.0f, INV_GREEN);
  }

  // ---- hit flash
  if (inv_hurt > 0) {
    int w = (int)(inv_hurt / 0.4f * 16.0f);
    for (int y = 0; y < IW; y++) for (int x = 0; x < IW; x++) fb[y * IW + x] = inv_mix565(fb[y * IW + x], 255, 30, 20, w);
  }

  // ---- end-of-level and game-over messages
  if (inv_state == INV_CLEAR) {
    char buf[16]; snprintf(buf, sizeof(buf), "LEVEL %d", inv_level);
    inv_text(fb, buf, 120, 78, 3, INV_GREEN);
    inv_text(fb, "CLEAR!", 120, 98, 3, INV_GREEN);
    inv_text(fb, inv_bonus >= INV_MAX_BONUS ? "BONUS FULL" : "+1 BONUS", 120, 128, 2, IC565(255, 230, 80));
  } else if (inv_state == INV_OVER) {
    for (int i = 0; i < IW * IW; i++) fb[i] = (uint16_t)((fb[i] >> 1) & 0x7BEF);
    inv_text(fb, "GAME OVER", 120, 78, 3, IC565(255, 90, 70));
    inv_text(fb, inv_overReason == INV_OVER_LANDED ? "AN ALIEN LANDED" : "OUT OF BONUS", 120, 104, 2, IC565(255, 230, 80));
    char buf[16]; snprintf(buf, sizeof(buf), "LEVEL %d", inv_level);
    inv_text(fb, buf, 120, 124, 2, INV_GREEN);
    inv_text(fb, "PRESS BOOT", 120, 150, 2, IC565(220, 220, 220));
  }
}

#pragma GCC pop_options
