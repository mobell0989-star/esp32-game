/*
  SKIBIDI GAME - TEST BUILD
  ESP32 + ST7735 1.77" (128x160)
  Test: di chuyển 360°, đấm A (combo tay trái/phải), skill nạp năng lượng + bắn cầu nổ

  Thư viện cần cài (Library Manager):
  - Adafruit GFX Library
  - Adafruit ST7735 and ST7789 Library
*/

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>

// ===================== CHÂN MÀN HÌNH =====================
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
// SCK = D18, MOSI = D23 (mặc định VSPI, không cần khai báo)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ===================== CHÂN NÚT =====================
#define BTN_UP          13
#define BTN_DOWN        12
#define BTN_LEFT        14
#define BTN_RIGHT       27
#define BTN_UPLEFT      26
#define BTN_UPRIGHT     25
#define BTN_DOWNLEFT    33
#define BTN_DOWNRIGHT   32

#define BTN_SKILL1      15
#define BTN_SKILL2      19
#define BTN_SKILL3      21
#define BTN_SKILL4      22
#define BTN_SKILL5      16
#define BTN_SKILL6      17

#define BTN_A           TX  // chân TX0
#define BTN_B           RX  // chân RX0

// ===================== MÀU SẮC =====================
#define COL_BG       ST77XX_WHITE
#define COL_BODY     0x219B   // xanh dương (giống ảnh)
#define COL_BODY_DK  0x10B4   // xanh đậm hơn cho tay/viền
#define COL_EYE      ST77XX_BLACK
#define COL_MOUTH    0x07FF   // cyan
#define COL_RED      ST77XX_RED
#define COL_FRAME    ST77XX_BLACK

// ===================== KÍCH THƯỚC MÀN HÌNH =====================
const int16_t SCR_W = 128;
const int16_t SCR_H = 160;
const int16_t FRAME = 2; // độ dày khung viền

// ===================== NHÂN VẬT =====================
// Thân 6x7 px (body), tay là 2x3 px mỗi bên, lúc nghỉ nằm sát 2 bên thân
float px, py;           // vị trí (float để di chuyển mượt, bo tròn khi vẽ)
float oldpx, oldpy;      // vị trí cũ để xóa
const int8_t BODY_W = 6;
const int8_t BODY_H = 7;
const int8_t ARM_W = 2;
const int8_t ARM_H = 3;

float facingX = 1, facingY = 0;  // hướng nhìn (vector đơn vị xấp xỉ)
float speed = 1.4;               // tốc độ di chuyển px/frame

// ===================== TAY ĐẤM (combo A) =====================
bool punching = false;
uint8_t punchSide = 0;      // 0 = tay phải trước, 1 = tay trái
int8_t punchFrame = 0;      // 0=nghỉ,1..3=đưa ra,4..6=thu lại
unsigned long lastPunchTime = 0;
const uint16_t PUNCH_FRAME_MS = 45; // tốc độ từng frame đấm (mượt)

// ===================== SKILL (nạp + bắn cầu đỏ) =====================
bool charging = false;
float chargeR = 3;          // bán kính vòng đỏ hiện tại
int8_t chargeDir = 1;       // 1 = phình ra, -1 = co lại
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

// vùng vẽ cũ của vòng nạp năng lượng (để xóa đúng chỗ, không full clear)
int16_t oldChargeBox_x, oldChargeBox_y, oldChargeBox_w, oldChargeBox_h;
bool chargeBoxDrawn = false;

// ===================== NÚT - ĐỌC TRẠNG THÁI =====================
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
Btn bSkill1    = {BTN_SKILL1, false, false};

void readBtn(Btn &b) {
  b.lastState = b.state;
  b.state = (digitalRead(b.pin) == LOW); // INPUT_PULLUP -> LOW khi nhấn
}

// ===================== SETUP =====================
void setup() {
  // Tắt WiFi/BT tiết kiệm RAM
  WiFi.mode(WIFI_OFF);
  btStop();

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UPLEFT, INPUT_PULLUP);
  pinMode(BTN_UPRIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWNLEFT, INPUT_PULLUP);
  pinMode(BTN_DOWNRIGHT, INPUT_PULLUP);
  pinMode(BTN_SKILL1, INPUT_PULLUP);
  pinMode(BTN_SKILL2, INPUT_PULLUP);
  pinMode(BTN_SKILL3, INPUT_PULLUP);
  pinMode(BTN_SKILL4, INPUT_PULLUP);
  pinMode(BTN_SKILL5, INPUT_PULLUP);
  pinMode(BTN_SKILL6, INPUT_PULLUP);
  // BTN_A (TX0) và BTN_B (RX0) dùng để upload code, set pinMode sau khi Serial không cần nữa
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB); // đổi thành INITR_GREENTAB nếu màu bị lệch
  tft.setRotation(0);
  tft.fillScreen(COL_BG);
  drawFrame();

  px = SCR_W / 2;
  py = SCR_H / 2;
  oldpx = px;
  oldpy = py;

  for (uint8_t i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;

  drawCharacter(px, py, facingX, facingY, false, 0);
}

// ===================== VẼ KHUNG VIỀN (vẽ 1 lần) =====================
void drawFrame() {
  tft.drawRect(0, 0, SCR_W, SCR_H, COL_FRAME);
  tft.drawRect(1, 1, SCR_W - 2, SCR_H - 2, COL_FRAME);
}

// ===================== VẼ NHÂN VẬT =====================
// fx, fy: hướng nhìn (để biết tay phải/trái nằm bên nào trên màn hình)
// isPunching: đang trong animation đấm
// frame: 0..6 frame đấm hiện tại
void eraseCharacter(float x, float y) {
  int16_t ex = (int16_t)x - BODY_W / 2 - ARM_W - 1;
  int16_t ey = (int16_t)y - BODY_H / 2 - 1;
  int16_t ew = BODY_W + ARM_W * 2 + 2;
  int16_t eh = BODY_H + 2;
  tft.fillRect(ex, ey, ew, eh, COL_BG);
  // vẽ lại khung viền nếu nhân vật đè lên viền
  if (ex <= 1 || ey <= 1 || ex + ew >= SCR_W - 1 || ey + eh >= SCR_H - 1) {
    drawFrame();
  }
}

void drawCharacter(float x, float y, float fx, float fy, bool isPunching, int8_t frame) {
  int16_t cx = (int16_t)x;
  int16_t cy = (int16_t)y;
  int16_t bx = cx - BODY_W / 2;
  int16_t by = cy - BODY_H / 2;

  // thân
  tft.fillRect(bx, by, BODY_W, BODY_H, COL_BODY);
  // mắt
  tft.drawPixel(bx + 1, by + 2, COL_EYE);
  tft.drawPixel(bx + 1, by + 3, COL_EYE);
  tft.drawPixel(bx + 4, by + 2, COL_EYE);
  tft.drawPixel(bx + 4, by + 3, COL_EYE);
  // miệng
  tft.drawFastHLine(bx + 1, by + 5, 4, COL_MOUTH);

  // xác định bên phải/trái dựa vào facing X (nếu facing nhìn trái thì đảo)
  bool facingRight = (fx >= 0);

  int8_t rightArmOffset = 0; // độ lệch ra ngoài khi đấm
  int8_t leftArmOffset = 0;

  if (isPunching) {
    // frame 0..2: tay đưa ra (offset tăng), 3..5: thu lại (offset giảm)
    int8_t outAmt;
    if (frame <= 2) outAmt = frame + 1;
    else outAmt = 6 - frame; // 3->3,4->2,5->1
    if (punchSide == 0) rightArmOffset = outAmt;
    else leftArmOffset = outAmt;
  }

  // tay phải (theo hướng facing)
  int16_t rax = facingRight ? (bx + BODY_W + rightArmOffset) : (bx - ARM_W - rightArmOffset);
  tft.fillRect(rax, by + 2, ARM_W, ARM_H, COL_BODY_DK);

  // tay trái
  int16_t lax = facingRight ? (bx - ARM_W - leftArmOffset) : (bx + BODY_W + leftArmOffset);
  tft.fillRect(lax, by + 2, ARM_W, ARM_H, COL_BODY_DK);
}

// ===================== XÓA + VẼ VÒNG NẠP NĂNG LƯỢNG =====================
void eraseChargeRing() {
  if (chargeBoxDrawn) {
    tft.fillRect(oldChargeBox_x, oldChargeBox_y, oldChargeBox_w, oldChargeBox_h, COL_BG);
    chargeBoxDrawn = false;
  }
}

void drawChargeRing(float x, float y, float r) {
  int16_t cx = (int16_t)x;
  int16_t cy = (int16_t)y;
  int16_t ir = (int16_t)(r * 0.55); // vòng trong đặc
  int16_t orad = (int16_t)r;        // vòng ngoài viền

  tft.fillCircle(cx, cy, ir, COL_RED);
  tft.drawCircle(cx, cy, orad, COL_RED);

  oldChargeBox_x = cx - orad - 1;
  oldChargeBox_y = cy - orad - 1;
  oldChargeBox_w = orad * 2 + 3;
  oldChargeBox_h = orad * 2 + 3;
  chargeBoxDrawn = true;
}

// ===================== ĐẠN (PROJECTILE) =====================
void fireProjectile(float x, float y, float dx, float dy) {
  proj.active = true;
  proj.x = x;
  proj.y = y;
  proj.oldx = x;
  proj.oldy = y;
  // chuẩn hóa hướng
  float len = sqrt(dx * dx + dy * dy);
  if (len < 0.01) { dx = 1; dy = 0; len = 1; }
  proj.vx = (dx / len) * PROJ_SPEED;
  proj.vy = (dy / len) * PROJ_SPEED;
}

void eraseProjectile() {
  tft.fillRect((int16_t)proj.oldx - 3, (int16_t)proj.oldy - 3, 7, 7, COL_BG);
  // vẽ lại frame nếu đạn gần viền
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
    // xóa vị trí cũ
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
  // --- đọc nút ---
  readBtn(bUp); readBtn(bDown); readBtn(bLeft); readBtn(bRight);
  readBtn(bUpLeft); readBtn(bUpRight); readBtn(bDownLeft); readBtn(bDownRight);
  readBtn(bA); readBtn(bB); readBtn(bSkill1);

  // --- tính hướng di chuyển (8 hướng + chuẩn hóa cho 360 mượt) ---
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

    // giới hạn trong khung
    int16_t half = BODY_W / 2 + ARM_W + 2;
    if (px < FRAME + half) px = FRAME + half;
    if (px > SCR_W - FRAME - half) px = SCR_W - FRAME - half;
    if (py < FRAME + BODY_H / 2 + 1) py = FRAME + BODY_H / 2 + 1;
    if (py > SCR_H - FRAME - BODY_H / 2 - 1) py = SCR_H - FRAME - BODY_H / 2 - 1;
  }

  // --- combo đấm A: giữ thì tự đánh phải->trái liên tục ---
  if (bA.state) {
    if (!punching) {
      punching = true;
      punchFrame = 0;
      punchSide = 0; // bắt đầu bằng tay phải
      lastPunchTime = millis();
    } else if (millis() - lastPunchTime >= PUNCH_FRAME_MS) {
      lastPunchTime = millis();
      punchFrame++;
      if (punchFrame > 5) {
        punchFrame = 0;
        punchSide = !punchSide; // đổi bên
      }
    }
  } else {
    punching = false;
    punchFrame = 0;
  }

  // --- skill nạp năng lượng (dùng SKILL1 hoặc B làm ví dụ test) ---
  bool skillHeld = bSkill1.state || bB.state;

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
      // vừa thả nút -> bắn cầu đỏ theo hướng facing
      charging = false;
      eraseChargeRing();
      if (!proj.active) {
        fireProjectile(px, py, facingX, facingY);
      }
      chargeR = CHARGE_MIN;
      chargeDir = 1;
    }
  }

  // ===================== VẼ LẠI (chỉ phần thay đổi) =====================

  // 1. xóa nhân vật vị trí cũ + xóa vòng nạp cũ nếu có
  bool needRedrawChar = moving || punching || (oldpx != px) || (oldpy != py);
  if (needRedrawChar) {
    eraseCharacter(oldpx, oldpy);
  }
  if (charging) {
    eraseChargeRing();
  }

  // 2. cập nhật đạn
  if (proj.active) {
    eraseProjectile();
    proj.x += proj.vx;
    proj.y += proj.vy;
    // va biên -> nổ
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

  // 3. vẽ lại nhân vật ở vị trí mới
  drawCharacter(px, py, facingX, facingY, punching, punchFrame);
  oldpx = px;
  oldpy = py;

  // 4. vẽ vòng nạp năng lượng nếu đang giữ skill
  if (charging) {
    drawChargeRing(px, py, chargeR);
  }

  // 5. vẽ đạn nếu còn bay
  if (proj.active) {
    drawProjectile();
  }

  // 6. cập nhật particle nổ
  updateParticles();

  delay(16); // ~60 FPS, mượt không giật
}
