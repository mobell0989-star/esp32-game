/*
  SKIBIDI GAME - TEST BUILD v2 (FIXED ST7735 WHITE SCREEN)
  ESP32 + ST7735 1.77" (128x160)
*/

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>

// ===================== CHÂN MÀN HÌNH =====================
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ===================== CHÂN NÚT DI CHUYỂN =====================
#define BTN_UP          13
#define BTN_DOWN        12
#define BTN_LEFT        14
#define BTN_RIGHT       27
#define BTN_UPLEFT      26
#define BTN_UPRIGHT     25
#define BTN_DOWNLEFT    33
#define BTN_DOWNRIGHT   32

// ===================== CHÂN NÚT SKILL =====================
#define BTN_SKILL1      15
#define BTN_SKILL2      19
#define BTN_SKILL3      21
#define BTN_SKILL4      22
#define BTN_SKILL5      16
#define BTN_SKILL6      17

#define BTN_A           1   // GPIO 1 (TX0)
#define BTN_B           3   // GPIO 3 (RX0)

// ===================== MÀU SẮC =====================
#define COL_BG       ST77XX_WHITE
#define COL_BODY     0x219B
#define COL_BODY_DK  0x10B4
#define COL_EYE      ST77XX_BLACK
#define COL_MOUTH    0x07FF
#define COL_RED      ST77XX_RED
#define COL_FRAME    ST77XX_BLACK

// ===================== KÍCH THƯỚC MÀN HÌNH =====================
const int16_t SCR_W = 128;
const int16_t SCR_H = 160;
const int16_t FRAME = 2;

// ===================== NHÂN VẬT =====================
float px, py;
float oldpx, oldpy;
const int8_t BODY_SIZE = 7;
const int8_t FIST_SIZE = 2;
const int8_t ARM_DIST  = 4;
const int8_t PUNCH_MAX = 4;

float facingX = 1, facingY = 0;
float speed = 1.4;

const int16_t MAX_REACH = (BODY_SIZE / 2) + ARM_DIST + PUNCH_MAX + FIST_SIZE + 1;

// ===================== TAY ĐẤM =====================
bool punching = false;
uint8_t punchSide = 0;
int8_t punchFrame = 0;
unsigned long lastPunchTime = 0;
const uint16_t PUNCH_FRAME_MS = 45;

// ===================== SKILL =====================
bool charging = false;
float chargeR = 3;
int8_t chargeDir = 1;
unsigned long lastChargeTime = 0;
const uint16_t CHARGE_FRAME_MS = 40;
const float CHARGE_MIN = 3, CHARGE_MAX = 8;

struct Projectile {
  bool active;
  float x, y;
  float vx, vy;
  float oldx, oldy;
};
Projectile proj = {false, 0, 0, 0, 0, 0, 0};
const float PROJ_SPEED = 4.0;

struct Particle {
  bool active;
  float x, y, vx, vy;
  int8_t life;
};
const uint8_t MAX_PARTICLES = 12;
Particle particles[MAX_PARTICLES];

int16_t oldChargeBox_x, oldChargeBox_y, oldChargeBox_w, oldChargeBox_h;
bool chargeBoxDrawn = false;

// ===================== NÚT =====================
struct Btn {
  uint8_t pin;
  bool state;
  bool lastState;
};

Btn bUp        = {BTN_UP, false, false};
Btn bDown      = {BTN_DOWN, false, false};
Btn bLeft      = {BTN_LEFT, false, false};
Btn bRight     = {BTN_RIGHT, false, false};
Btn bUpLeft    = {BTN_UPLEFT, false, false};
Btn bUpRight   = {BTN_UPRIGHT, false, false};
Btn bDownLeft  = {BTN_DOWNLEFT, false, false};
Btn bDownRight = {BTN_DOWNRIGHT, false, false};
Btn bA         = {BTN_A, false, false};
Btn bB         = {BTN_B, false, false};

Btn bSkill[6] = {
  {BTN_SKILL1, false, false},
  {BTN_SKILL2, false, false},
  {BTN_SKILL3, false, false},
  {BTN_SKILL4, false, false},
  {BTN_SKILL5, false, false},
  {BTN_SKILL6, false, false}
};

void readBtn(Btn &b) {
  b.lastState = b.state;
  b.state = (digitalRead(b.pin) == LOW);
}

// ===================== SETUP =====================
void setup() {
  WiFi.mode(WIFI_OFF);
  btStop();

  // Reset cứng màn hình qua phần cứng trước khi khởi tạo
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(10);
  digitalWrite(TFT_RST, HIGH);
  delay(10);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UPLEFT, INPUT_PULLUP);
  pinMode(BTN_UPRIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWNLEFT, INPUT_PULLUP);
  pinMode(BTN_DOWNRIGHT, INPUT_PULLUP);
  for (uint8_t i = 0; i < 6; i++) pinMode(bSkill[i].pin, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);

  // Đổi sang INTI_GREENTAB - Khắc phục màn hình trắng cho ST7735 1.77"
  tft.initR(INITR_GREENTAB); 
  tft.setRotation(2);
  tft.fillScreen(COL_BG);
  drawFrame();

  px = SCR_W / 2;
  py = SCR_H / 2;
  oldpx = px;
  oldpy = py;

  for (uint8_t i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;

  drawCharacter(px, py, facingX, facingY, false, 0, 0);
}

// ===================== VẼ KHUNG VIỀN =====================
void drawFrame() {
  tft.drawRect(0, 0, SCR_W, SCR_H, COL_FRAME);
  tft.drawRect(1, 1, SCR_W - 2, SCR_H - 2, COL_FRAME);
}

// ===================== XÓA NHÂN VẬT =====================
void eraseCharacter(float x, float y) {
  int16_t ex = (int16_t)x - MAX_REACH;
  int16_t ey = (int16_t)y - MAX_REACH;
  int16_t esz = MAX_REACH * 2;
  tft.fillRect(ex, ey, esz, esz, COL_BG);
  if (ex <= 1 || ey <= 1 || ex + esz >= SCR_W - 1 || ey + esz >= SCR_H - 1) {
    drawFrame();
  }
}

// ===================== VẼ NHÂN VẬT =====================
void drawCharacter(float x, float y, float fx, float fy, bool isPunching, uint8_t side, int8_t frame) {
  int16_t cx = (int16_t)x;
  int16_t cy = (int16_t)y;
  int16_t bx = cx - BODY_SIZE / 2;
  int16_t by = cy - BODY_SIZE / 2;

  tft.fillRect(bx, by, BODY_SIZE, BODY_SIZE, COL_BODY);
  tft.drawPixel(bx + 1, by + 2, COL_EYE);
  tft.drawPixel(bx + 1, by + 3, COL_EYE);
  tft.drawPixel(bx + 5, by + 2, COL_EYE);
  tft.drawPixel(bx + 5, by + 3, COL_EYE);
  tft.drawFastHLine(bx + 1, by + 5, 5, COL_MOUTH);

  float perpX = -fy;
  float perpY = fx;

  float rightIdleX = x + (-perpX) * ARM_DIST - fx * 1.0;
  float rightIdleY = y + (-perpY) * ARM_DIST - fy * 1.0;
  float leftIdleX  = x + (perpX)  * ARM_DIST - fx * 1.0;
  float leftIdleY  = y + (perpY)  * ARM_DIST - fy * 1.0;

  float extendR = 0, extendL = 0;
  if (isPunching) {
    int8_t outAmt;
    if (frame <= 2) outAmt = ((frame + 1) * PUNCH_MAX) / 3;
    else            outAmt = ((6 - frame) * PUNCH_MAX) / 3;
    if (side == 0) extendR = outAmt;
    else            extendL = outAmt;
  }

  int16_t rfx = (int16_t)(rightIdleX + fx * extendR) - FIST_SIZE / 2;
  int16_t rfy = (int16_t)(rightIdleY + fx * extendR) - FIST_SIZE / 2;
  tft.fillRect(rfx, rfy, FIST_SIZE, FIST_SIZE, COL_BODY_DK);

  int16_t lfx = (int16_t)(leftIdleX + fx * extendL) - FIST_SIZE / 2;
  int16_t lfy = (int16_t)(leftIdleY + fx * extendL) - FIST_SIZE / 2;
  tft.fillRect(lfx, lfy, FIST_SIZE, FIST_SIZE, COL_BODY_DK);
}

// ===================== VÒNG NẠP NĂNG LƯỢNG =====================
void eraseChargeRing() {
  if (chargeBoxDrawn) {
    tft.fillRect(oldChargeBox_x, oldChargeBox_y, oldChargeBox_w, oldChargeBox_h, COL_BG);
    chargeBoxDrawn = false;
  }
}

void drawChargeRing(float x, float y, float r) {
  int16_t cx = (int16_t)x;
  int16_t cy = (int16_t)y;
  int16_t ir = (int16_t)(r * 0.55);
  int16_t orad = (int16_t)r;

  tft.fillCircle(cx, cy, ir, COL_RED);
  tft.drawCircle(cx, cy, orad, COL_RED);

  oldChargeBox_x = cx - orad - 1;
  oldChargeBox_y = cy - orad - 1;
  oldChargeBox_w = orad * 2 + 3;
  oldChargeBox_h = orad * 2 + 3;
  chargeBoxDrawn = true;
}

// ===================== ĐẠN =====================
void fireProjectile(float x, float y, float dx, float dy) {
  proj.active = true;
  proj.x = x;
  proj.y = y;
  proj.oldx = x;
  proj.oldy = y;
  float len = sqrt(dx * dx + dy * dy);
  if (len < 0.01) { dx = 1; dy = 0; len = 1; }
  proj.vx = (dx / len) * PROJ_SPEED;
  proj.vy = (dy / len) * PROJ_SPEED;
}

void eraseProjectile() {
  tft.fillRect((int16_t)proj.oldx - 3, (int16_t)proj.oldy - 3, 7, 7, COL_BG);
  if (proj.oldx < 4 || proj.oldy < 4 || proj.oldx > SCR_W - 4 || proj.oldy > SCR_H - 4) {
    drawFrame();
  }
}

void drawProjectile() {
  int16_t cx = (int16_t)proj.x;
  int16_t cy = (int16_t)proj.y;
  tft.fillCircle(cx, cy, 2, COL_RED);
  tft.drawPixel(cx - 2, cy, 0xF800);
  tft.drawPixel(cx + 2, cy, 0xF800);
}

void explodeProjectile(float x, float y) {
  for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
    particles[i].active = true;
    particles[i].x = x;
    particles[i].y = y;
    float ang = (TWO_PI / MAX_PARTICLES) * i;
    particles[i].vx = cos(ang) * 1.6;
    particles[i].vy = sin(ang) * 1.6;
    particles[i].life = 10;
  }
}

void updateParticles() {
  for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) continue;
    tft.drawPixel((int16_t)particles[i].x, (int16_t)particles[i].y, COL_BG);
    particles[i].x += particles[i].vx;
    particles[i].y += particles[i].vy;
    particles[i].life--;
    if (particles[i].life <= 0 ||
        particles[i].x < 1 || particles[i].x > SCR_W - 2 ||
        particles[i].y < 1 || particles[i].y > SCR_H - 2) {
      particles[i].active = false;
    } else {
      tft.drawPixel((int16_t)particles[i].x, (int16_t)particles[i].y, COL_RED);
    }
  }
}

// ===================== LOOP =====================
void loop() {
  readBtn(bUp); readBtn(bDown); readBtn(bLeft); readBtn(bRight);
  readBtn(bUpLeft); readBtn(bUpRight); readBtn(bDownLeft); readBtn(bDownRight);
  readBtn(bA); readBtn(bB);
  for (uint8_t i = 0; i < 6; i++) readBtn(bSkill[i]);

  float dx = 0, dy = 0;
  if (bUp.state)        dy -= 1;
  if (bDown.state)      dy += 1;
  if (bLeft.state)       dx -= 1;
  if (bRight.state)      dx += 1;
  if (bUpLeft.state)    { dx -= 1; dy -= 1; }
  if (bUpRight.state)   { dx += 1; dy -= 1; }
  if (bDownLeft.state)  { dx -= 1; dy += 1; }
  if (bDownRight.state) { dx += 1; dy += 1; }

  bool moving = (dx != 0 || dy != 0);
  if (moving) {
    float len = sqrt(dx * dx + dy * dy);
    dx /= len; dy /= len;
    facingX = dx;
    facingY = dy;
    px += dx * speed;
    py += dy * speed;

    if (px < FRAME + MAX_REACH) px = FRAME + MAX_REACH;
    if (px > SCR_W - FRAME - MAX_REACH) px = SCR_W - FRAME - MAX_REACH;
    if (py < FRAME + MAX_REACH) py = FRAME + MAX_REACH;
    if (py > SCR_H - FRAME - MAX_REACH) py = SCR_H - FRAME - MAX_REACH;
  }

  if (bA.state) {
    if (!punching) {
      punching = true;
      punchFrame = 0;
      punchSide = 0;
      lastPunchTime = millis();
    } else if (millis() - lastPunchTime >= PUNCH_FRAME_MS) {
      lastPunchTime = millis();
      punchFrame++;
      if (punchFrame > 5) {
        punchFrame = 0;
        punchSide = !punchSide;
      }
    }
  } else {
    punching = false;
    punchFrame = 0;
  }

  bool skillHeld = false;
  for (uint8_t i = 0; i < 6; i++) {
    if (bSkill[i].state) { skillHeld = true; break; }
  }

  if (skillHeld) {
    charging = true;
    if (millis() - lastChargeTime >= CHARGE_FRAME_MS) {
      lastChargeTime = millis();
      chargeR += chargeDir * 0.6;
      if (chargeR >= CHARGE_MAX) chargeDir = -1;
      if (chargeR <= CHARGE_MIN) chargeDir = 1;
    }
  } else {
    if (charging) {
      charging = false;
      eraseChargeRing();
      if (!proj.active) {
        fireProjectile(px, py, facingX, facingY);
      }
      chargeR = CHARGE_MIN;
      chargeDir = 1;
    }
  }

  bool needRedrawChar = moving || punching || (oldpx != px) || (oldpy != py);
  if (needRedrawChar) {
    eraseCharacter(oldpx, oldpy);
  }
  if (charging) {
    eraseChargeRing();
  }

  if (proj.active) {
    eraseProjectile();
    proj.x += proj.vx;
    proj.y += proj.vy;
    if (proj.x <= FRAME + 2 || proj.x >= SCR_W - FRAME - 2 ||
        proj.y <= FRAME + 2 || proj.y >= SCR_H - FRAME - 2) {
      explodeProjectile(proj.x, proj.y);
      proj.active = false;
      drawFrame();
    } else {
      proj.oldx = proj.x;
      proj.oldy = proj.y;
    }
  }

  drawCharacter(px, py, facingX, facingY, punching, punchSide, punchFrame);
  oldpx = px;
  oldpy = py;

  if (charging) {
    drawChargeRing(px, py, chargeR);
  }

  if (proj.active) {
    drawProjectile();
  }

  updateParticles();

  delay(16);
}
