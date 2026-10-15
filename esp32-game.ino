/*
  SKIBIDI OPEN WORLD - ESP32 + ST7735 128x160
  - Conifer trees on both sides of a road
  - Square player can move in 8 directions
  - Scrolling scenery gives a simple open-world walking feel
  - No image files needed: trees are drawn as pixel-art shapes in code

  Libraries:
    Adafruit GFX Library
    Adafruit ST7735 and ST7789 Library

  Display wiring (as in your earlier setup):
    TFT_CS  = 5
    TFT_DC  = 2
    TFT_RST = 4
    TFT_SCLK= 18
    TFT_MOSI= 23

  Button pins (each button connects its pin to GND when pressed):
    UP=13, DOWN=12, LEFT=14, RIGHT=27
    UP_LEFT=26, UP_RIGHT=25, DOWN_LEFT=33, DOWN_RIGHT=32

  If your buttons are wired differently, change the pin constants below.
*/

#include <SPI.h>
#include <WiFi.h>
#include "esp_bt.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Movement buttons, active LOW (INPUT_PULLUP)
const uint8_t BTN_UP        = 13;
const uint8_t BTN_DOWN      = 12;
const uint8_t BTN_LEFT      = 14;
const uint8_t BTN_RIGHT     = 27;
const uint8_t BTN_UP_LEFT   = 26;
const uint8_t BTN_UP_RIGHT  = 25;
const uint8_t BTN_DOWN_LEFT = 33;
const uint8_t BTN_DOWN_RIGHT= 32;

const int SCREEN_W = 128;
const int SCREEN_H = 160;

// RGB565 colors
const uint16_t SKY       = 0xDFFF;
const uint16_t GRASS     = 0x45E5;
const uint16_t GRASS_DK  = 0x2D43;
const uint16_t ROAD      = 0x7BEF;
const uint16_t ROAD_DK   = 0x528A;
const uint16_t ROAD_EDGE = 0xD69A;
const uint16_t WHITE     = 0xFFFF;
const uint16_t BLACK     = 0x0000;
const uint16_t TREE_DK   = 0x0925;
const uint16_t TREE_MID  = 0x1D86;
const uint16_t TREE_LIT  = 0x3D68;
const uint16_t TRUNK     = 0x79E4;
const uint16_t TRUNK_DK  = 0x4942;
const uint16_t PLAYER    = 0xFBE0;
const uint16_t PLAYER_DK = 0xA145;

float playerX = 64.0f;
float playerY = 122.0f;
const float PLAYER_SPEED = 2.2f;

// World offset advances when the player walks vertically.
// Tree positions are stored in world coordinates and wrap around.
const int TREE_COUNT = 18;
float treeWorldY[TREE_COUNT];
int treeSide[TREE_COUNT];   // -1 left, +1 right
int treeDepth[TREE_COUNT];  // 0 far/small, 1 mid, 2 near/large
float worldOffset = 0.0f;
unsigned long lastFrame = 0;

bool pressed(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

void setupButtons() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UP_LEFT, INPUT_PULLUP);
  pinMode(BTN_UP_RIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWN_LEFT, INPUT_PULLUP);
  pinMode(BTN_DOWN_RIGHT, INPUT_PULLUP);
}

void initTrees() {
  // Spread trees along the visible world, alternating sides.
  for (int i = 0; i < TREE_COUNT; i++) {
    treeWorldY[i] = i * 24.0f + random(-8, 9);
    treeSide[i] = (i % 2 == 0) ? -1 : 1;
    treeDepth[i] = random(0, 3);
  }
}

void drawTree(int x, int y, int scale) {
  // Tiny pixel-art evergreen inspired by the supplied tree.
  // scale 1 = 1x pixel blocks; scale 2 = larger, chunky pixels.
  int p = scale;
  int top = y;
  int cx = x;

  // Tip
  tft.fillRect(cx - p/2, top, p, 3*p, TREE_DK);
  tft.fillRect(cx - p/2, top + p, p, p, TREE_LIT);

  // Three layered branches, each wider than the one above.
  int layerY[3] = {top + 3*p, top + 7*p, top + 12*p};
  int halfW[3]  = {3*p, 6*p, 9*p};

  for (int layer = 0; layer < 3; layer++) {
    int yy = layerY[layer];
    int hw = halfW[layer];
    for (int row = 0; row < 5; row++) {
      int half = hw - (row * hw / 5);
      if (half < p) half = p;
      tft.fillRect(cx - half, yy + row*p, half*2 + p, p, TREE_MID);
      // Shaded right edge and scattered highlights create pixel-art texture.
      tft.fillRect(cx + half/3, yy + row*p, half - half/3 + p, p, TREE_DK);
      if (row == 1 || row == 3) {
        tft.fillRect(cx - half + p, yy + row*p, p, p, TREE_LIT);
      }
    }
    // A few dark branch pixels.
    tft.fillRect(cx - p, yy + 2*p, p, p, TREE_DK);
    tft.fillRect(cx + hw/2, yy + 3*p, p, p, TREE_DK);
  }

  // Trunk and ground shadow
  int trunkY = top + 17*p;
  tft.fillRect(cx - 2*p, trunkY, 4*p, 6*p, TRUNK);
  tft.fillRect(cx, trunkY + p, 2*p, 5*p, TRUNK_DK);
  tft.fillRect(cx - 5*p, trunkY + 6*p, 10*p, p, ROAD_DK);
}

void drawBackground() {
  tft.fillScreen(SKY);

  // Grass fields
  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, GRASS);

  // Subtle grass pixels
  for (int i = 0; i < 90; i++) {
    int gx = (i * 37 + 11) % SCREEN_W;
    int gy = (i * 53 + 7) % SCREEN_H;
    if (gx < 31 || gx > 97) {
      tft.drawPixel(gx, gy, (i % 3 == 0) ? GRASS_DK : 0x3D84);
    }
  }

  // Road in the center, with shoulders.
  tft.fillRect(28, 0, 72, SCREEN_H, ROAD_EDGE);
  tft.fillRect(31, 0, 66, SCREEN_H, ROAD);
  tft.drawFastVLine(32, 0, SCREEN_H, ROAD_DK);
  tft.drawFastVLine(95, 0, SCREEN_H, ROAD_DK);

  // Dashed center line scrolls as the world moves.
  int dashOffset = ((int)worldOffset) % 28;
  for (int y = -28 + dashOffset; y < SCREEN_H; y += 28) {
    tft.fillRect(62, y, 4, 15, WHITE);
  }

  // Trees on both sides; distant trees are smaller.
  for (int i = 0; i < TREE_COUNT; i++) {
    float screenY = treeWorldY[i] - worldOffset;
    while (screenY < -50) screenY += TREE_COUNT * 24.0f;
    while (screenY > SCREEN_H + 25) screenY -= TREE_COUNT * 24.0f;
    if (screenY < -25 || screenY > SCREEN_H + 5) continue;

    int scale = (treeDepth[i] == 0) ? 1 : ((treeDepth[i] == 1) ? 2 : 2);
    int x;
    if (treeSide[i] < 0) {
      x = (treeDepth[i] == 0) ? 13 : ((treeDepth[i] == 1) ? 17 : 21);
    } else {
      x = (treeDepth[i] == 0) ? 114 : ((treeDepth[i] == 1) ? 110 : 106);
    }
    drawTree(x, (int)screenY, scale);
  }
}

void drawPlayer() {
  int x = (int)playerX;
  int y = (int)playerY;

  // Small shadow, square character, and simple face.
  tft.fillRect(x - 5, y + 7, 11, 3, ROAD_DK);
  tft.fillRect(x - 4, y - 4, 9, 9, PLAYER_DK);
  tft.fillRect(x - 3, y - 5, 9, 9, PLAYER);
  tft.drawPixel(x - 1, y - 2, BLACK);
  tft.drawPixel(x + 3, y - 2, BLACK);
  tft.drawFastHLine(x - 1, y + 1, 4, BLACK);
}

void readMovement(float &dx, float &dy) {
  dx = 0;
  dy = 0;

  bool up = pressed(BTN_UP) || pressed(BTN_UP_LEFT) || pressed(BTN_UP_RIGHT);
  bool down = pressed(BTN_DOWN) || pressed(BTN_DOWN_LEFT) || pressed(BTN_DOWN_RIGHT);
  bool left = pressed(BTN_LEFT) || pressed(BTN_UP_LEFT) || pressed(BTN_DOWN_LEFT);
  bool right = pressed(BTN_RIGHT) || pressed(BTN_UP_RIGHT) || pressed(BTN_DOWN_RIGHT);

  if (up && !down) dy = -PLAYER_SPEED;
  if (down && !up) dy = PLAYER_SPEED;
  if (left && !right) dx = -PLAYER_SPEED;
  if (right && !left) dx = PLAYER_SPEED;

  // Normalize diagonal movement so it isn't faster.
  if (dx != 0 && dy != 0) {
    dx *= 0.7071f;
    dy *= 0.7071f;
  }
}

void updateWorld(float dx, float dy) {
  playerX += dx;
  playerY += dy;

  // Keep player on screen. The scenery scrolls when moving vertically.
  if (playerX < 5) playerX = 5;
  if (playerX > SCREEN_W - 5) playerX = SCREEN_W - 5;
  if (playerY < 12) {
    playerY = 12;
    worldOffset -= PLAYER_SPEED;
  }
  if (playerY > SCREEN_H - 9) {
    playerY = SCREEN_H - 9;
    worldOffset += PLAYER_SPEED;
  }

  // Also shift the world gently when walking left/right, like exploring.
  // Road remains centered, so lateral movement is limited to the scene.
  if (playerX < 34) playerX = 34;
  if (playerX > 94) playerX = 94;

  if (worldOffset < 0) worldOffset += TREE_COUNT * 24.0f;
  if (worldOffset >= TREE_COUNT * 24.0f) worldOffset -= TREE_COUNT * 24.0f;
}

void setup() {
  // Keep Wi-Fi/Bluetooth off to save memory and power.
  WiFi.mode(WIFI_OFF); // WiFi.h is included below by ESP32 Arduino core
  btStop();

  randomSeed((uint32_t)esp_random());
  setupButtons();

  tft.initR(INITR_GREENTAB);
  tft.setRotation(2);
  tft.fillScreen(BLACK);

  initTrees();
  drawBackground();
  drawPlayer();
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrame < 35) return; // about 28 FPS
  lastFrame = now;

  float dx, dy;
  readMovement(dx, dy);
  updateWorld(dx, dy);

  drawBackground();
  drawPlayer();
}
