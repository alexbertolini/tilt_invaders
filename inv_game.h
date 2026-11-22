// inv_game.h - game logic for the 3D space invaders (pure C++, no Arduino dependencies)
//
// World: the player stands at the origin, eye height INV_EYE_H, looking along +z when yaw = 0.
// x = right, y = up, z = forward (all in metres). The invaders hang in the sky on an arc around the player.
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>

#define INV_MAXA 60          // most invaders on screen (level 60 and beyond stay at 60)
#define INV_MAXB 6           // most alien bolts in flight

enum { INV_PLAY = 0, INV_CLEAR = 1, INV_OVER = 2 };
enum { INV_OVER_HITS = 1, INV_OVER_LANDED = 2 };

// ---------------- tuning ----------------
static const float INV_EYE_H     = 2.0f;     // eye height above the ground (m)
static const float INV_PIX       = 0.7f;     // world size of one sprite pixel (m)
static const float INV_F         = 171.0f;   // focal length in pixels (70 degrees across the 240 px screen)
static const float INV_SP_X      = 11.0f;    // spacing between invaders in a row (m)
static const float INV_SP_Y      = 8.5f;     // spacing between rows (m)
static const float INV_R0        = 52.0f;    // starting distance of the formation (m)
static const float INV_Y0        = 26.0f;    // starting altitude of the bottom row (m)
static const float INV_DR        = 2.4f;     // each step: this much closer ...
static const float INV_DY        = 3.2f;     // ... and this much closer to the ground
static const float INV_THETA_MAX = 0.77f;    // the formation turns around when an invader reaches 44 degrees left/right
static const float INV_HIT_ANGLE = 0.115f;   // an alien bolt hits if the crosshair is within 6.6 degrees of it on arrival
static const int   INV_MAX_BONUS = 10;
static const int   INV_START_BONUS = 3;
static const float INV_LASER_COOLDOWN = 0.22f;

// ---------------- state ----------------
struct InvAlien { float x, y, z; uint8_t type, hp; float expl, flash; };
struct InvBolt  { float ox, oy, oz, t, T; bool on; };

static InvAlien inv_a[INV_MAXA];
static int      inv_na = 0;                  // number of invaders in this level
static InvBolt  inv_b[INV_MAXB];
static int      inv_state = INV_PLAY, inv_level = 1, inv_bonus = INV_START_BONUS, inv_overReason = 0;
static float    inv_theta = 0, inv_R = INV_R0, inv_Y = INV_Y0, inv_tR = INV_R0, inv_tY = INV_Y0, inv_dir = 1, inv_pause = 0;
static float    inv_fireT = 2, inv_clearT = 0, inv_laserT = 0, inv_cool = 0, inv_hurt = 0, inv_spark = 0, inv_time = 0;
static float    inv_yaw = 0, inv_pitch = -1.5707963f;
static uint32_t inv_rng = 2463534242u;

static float inv_rand() {                    // xorshift32 in [0,1)
  inv_rng ^= inv_rng << 13; inv_rng ^= inv_rng >> 17; inv_rng ^= inv_rng << 5;
  return (float)(inv_rng & 0xFFFFFF) / 16777216.0f;
}
static inline float inv_clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static int inv_alive() { int n = 0; for (int i = 0; i < inv_na; i++) if (inv_a[i].hp > 0) n++; return n; }

// place every invader from the formation parameters
static void inv_place() {
  float st = sinf(inv_theta), ct = cosf(inv_theta);
  for (int i = 0; i < inv_na; i++) {
    int r = i / 5, c = i - 5 * r;
    int rowCount = inv_na - 5 * r; if (rowCount > 5) rowCount = 5;
    float dx = ((float)c - (float)(rowCount - 1) * 0.5f) * INV_SP_X;
    inv_a[i].x = inv_R * st + dx * ct;
    inv_a[i].z = inv_R * ct - dx * st;
    inv_a[i].y = inv_Y + (float)r * INV_SP_Y;
  }
}

static void inv_start_level(int level) {
  inv_level = level;
  inv_na = level > INV_MAXA ? INV_MAXA : level;                  // level n has n invaders
  for (int i = 0; i < inv_na; i++) {
    int r = i / 5, k = r % 3;
    inv_a[i].type = (k == 0) ? 2 : (k == 1 ? 1 : 0);              // octopus at the bottom, then crabs, then squids
    inv_a[i].hp = 2; inv_a[i].expl = 0; inv_a[i].flash = 0;
  }
  inv_theta = 0; inv_R = inv_tR = INV_R0; inv_Y = inv_tY = INV_Y0;
  inv_dir = inv_rand() < 0.5f ? -1.0f : 1.0f;
  inv_pause = 1.2f; inv_fireT = 2.5f;
  for (int i = 0; i < INV_MAXB; i++) inv_b[i].on = false;
  inv_state = INV_PLAY; inv_laserT = inv_cool = inv_hurt = inv_spark = 0;
  inv_place();
}

static void inv_new_game(uint32_t seed) {
  if (seed) inv_rng = seed;
  inv_bonus = INV_START_BONUS; inv_overReason = 0;
  inv_start_level(1);
}

static void inv_hurt_player() {
  inv_bonus--;
  inv_hurt = 0.4f;
  if (inv_bonus <= 0) { inv_bonus = 0; inv_state = INV_OVER; inv_overReason = INV_OVER_HITS; }
}

// forward vector of the camera
static void inv_forward(float &fx, float &fy, float &fz) {
  float cp = cosf(inv_pitch);
  fx = sinf(inv_yaw) * cp; fy = sinf(inv_pitch); fz = cosf(inv_yaw) * cp;
}

// The player presses the fire button: a laser along the crosshair. Returns 0 = nothing, 1 = hit, 2 = invader destroyed.
static int inv_fire() {
  if (inv_state != INV_PLAY || inv_cool > 0.0f) return 0;
  inv_cool = INV_LASER_COOLDOWN; inv_laserT = 0.12f;
  float cy = cosf(inv_yaw), sy = sinf(inv_yaw), cp = cosf(inv_pitch), sp = sinf(inv_pitch);
  float fx = sy * cp, fy = sp, fz = cy * cp, rx = cy, rz = -sy, ux = -sy * sp, uy = cp, uz = -cy * sp;
  int best = -1; float bestZ = 1e9f;
  for (int i = 0; i < inv_na; i++) {
    if (inv_a[i].hp == 0) continue;
    float px = inv_a[i].x, py = inv_a[i].y - INV_EYE_H, pz = inv_a[i].z;
    float zc = px * fx + py * fy + pz * fz;
    if (zc < 1.0f) continue;
    float xc = px * rx + pz * rz, yc = px * ux + py * uy + pz * uz;
    float sx = INV_F * xc / zc, sy2 = INV_F * yc / zc;                 // screen offset from the crosshair (px)
    int w = 0; { static const int W[3] = {8, 11, 12}; w = W[inv_a[i].type]; }
    float hw = 0.5f * (float)w * INV_PIX * INV_F / zc + 3.0f, hh = 4.0f * INV_PIX * INV_F / zc + 3.0f;
    if (fabsf(sx) <= hw && fabsf(sy2) <= hh && zc < bestZ) { best = i; bestZ = zc; }
  }
  if (best < 0) return 0;
  inv_spark = 0.2f;
  inv_a[best].flash = 0.2f;
  if (--inv_a[best].hp == 0) { inv_a[best].expl = 0.45f; return 2; }
  return 1;
}

static void inv_update(float dt, float yaw, float pitch) {
  inv_yaw = yaw; inv_pitch = pitch; inv_time += dt;
  if (inv_laserT > 0) inv_laserT -= dt;
  if (inv_cool > 0) inv_cool -= dt;
  if (inv_hurt > 0) inv_hurt -= dt;
  if (inv_spark > 0) inv_spark -= dt;
  for (int i = 0; i < inv_na; i++) {
    if (inv_a[i].flash > 0) inv_a[i].flash -= dt;
    if (inv_a[i].hp == 0 && inv_a[i].expl > 0) inv_a[i].expl -= dt;
  }
  if (inv_state == INV_CLEAR) {
    inv_clearT -= dt;
    if (inv_clearT <= 0) inv_start_level(inv_level + 1);
    return;
  }
  if (inv_state != INV_PLAY) return;

  int alive = inv_alive();

  // ---- the formation: shift sideways, and when an invader reaches the edge come a little closer to the ground
  float total = (float)inv_na;
  int lv = inv_level > 30 ? 30 : inv_level;
  float omega = (8.0f + 0.35f * (float)lv) * 0.0174533f * (1.0f + 1.6f * (1.0f - (float)alive / total));
  if (inv_pause > 0) inv_pause -= dt; else inv_theta += inv_dir * omega * dt;
  const float descentRate = 6.0f;
  inv_R += inv_clampf(inv_tR - inv_R, -descentRate * dt, descentRate * dt);
  inv_Y += inv_clampf(inv_tY - inv_Y, -descentRate * dt, descentRate * dt);
  inv_place();
  if (inv_pause <= 0 && alive > 0) {
    float amin = 1e9f, amax = -1e9f;
    for (int i = 0; i < inv_na; i++) {
      if (inv_a[i].hp == 0) continue;
      float az = atan2f(inv_a[i].x, inv_a[i].z);
      if (az < amin) amin = az;
      if (az > amax) amax = az;
    }
    if ((inv_dir > 0 && amax > INV_THETA_MAX) || (inv_dir < 0 && amin < -INV_THETA_MAX)) {
      inv_dir = -inv_dir;
      inv_tR -= INV_DR; inv_tY -= INV_DY;
      if (inv_tR < 14.0f) inv_tR = 14.0f;
      inv_pause = 0.55f;
    }
  }

  // ---- landing: the lowest invader touches the ground
  for (int i = 0; i < inv_na; i++)
    if (inv_a[i].hp > 0 && inv_a[i].y - 4.0f * INV_PIX <= 0.0f) { inv_state = INV_OVER; inv_overReason = INV_OVER_LANDED; return; }

  // ---- the invaders shoot straight at the player
  inv_fireT -= dt;
  if (inv_fireT <= 0 && alive > 0) {
    int pick = (int)(inv_rand() * (float)alive); if (pick >= alive) pick = alive - 1;
    int idx = -1;
    for (int i = 0, k = 0; i < inv_na; i++) if (inv_a[i].hp > 0) { if (k == pick) { idx = i; break; } k++; }
    for (int b = 0; b < INV_MAXB && idx >= 0; b++) {
      if (inv_b[b].on) continue;
      int lv2 = inv_level > 40 ? 40 : inv_level;
      inv_b[b].ox = inv_a[idx].x; inv_b[b].oy = inv_a[idx].y; inv_b[b].oz = inv_a[idx].z;
      inv_b[b].t = 0; inv_b[b].T = 1.9f - 0.02f * (float)lv2; inv_b[b].on = true;
      inv_a[idx].flash = 0.3f;
      break;
    }
    float interval = 3.2f / (1.0f + 0.18f * sqrtf((float)alive));
    inv_fireT = interval * (0.6f + 0.8f * inv_rand());
  }

  // ---- bolts: they hit only if the crosshair points at them when they arrive
  float fx, fy, fz; inv_forward(fx, fy, fz);
  const float cosHit = cosf(INV_HIT_ANGLE);
  for (int b = 0; b < INV_MAXB; b++) {
    if (!inv_b[b].on) continue;
    inv_b[b].t += dt;
    if (inv_b[b].t < inv_b[b].T) continue;
    inv_b[b].on = false;
    float dx = inv_b[b].ox, dy = inv_b[b].oy - INV_EYE_H, dz = inv_b[b].oz;
    float len = sqrtf(dx * dx + dy * dy + dz * dz) + 0.001f;
    float cosang = (dx * fx + dy * fy + dz * fz) / len;
    if (cosang >= cosHit && inv_state == INV_PLAY) inv_hurt_player();
  }
  if (inv_state != INV_PLAY) return;

  // ---- level cleared: an extra bonus, then the next level has one more invader
  bool exploding = false;
  for (int i = 0; i < inv_na; i++) if (inv_a[i].hp == 0 && inv_a[i].expl > 0) exploding = true;
  if (alive == 0 && !exploding) {
    inv_state = INV_CLEAR; inv_clearT = 2.4f;
    if (inv_bonus < INV_MAX_BONUS) inv_bonus++;
    for (int b = 0; b < INV_MAXB; b++) inv_b[b].on = false;
  }
}
