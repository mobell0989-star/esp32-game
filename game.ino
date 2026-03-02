#include <SPI.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Buttons
const int btnUp = 12;
const int btnDown = 14;
const int btnLeft = 27;
const int btnRight = 26;
const int btnAtk = 25;

// Player
int playerX = 80;
int playerY = 64;

// Boss
float bossX = 30;
float bossY = 30;
int bossHP = 40;
bool bossAlive = true;

// Attack
bool attacking = false;
int attackTimer = 0;
float attackDirX = 1;
float attackDirY = 0;

void setup() {

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_WHITE);

  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);
  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnAtk, INPUT_PULLUP);
}

void drawBossHP() {
  int size = 4;
  int gap = 1;
  int startX = 5;
  int startY = 5;

  tft.fillRect(startX, startY, 120, 12, ST77XX_WHITE);

  for(int i = 0; i < 40; i++) {
    int row = i / 20;
    int col = i % 20;

    int x = startX + col * (size + gap);
    int y = startY + row * (size + gap + 1);

    if(i < bossHP)
      tft.fillRect(x, y, size, size, ST77XX_RED);
    else
      tft.fillRect(x, y, size, size, 0xC618);
  }
}

void loop() {

  if(!bossAlive) return;

  tft.fillScreen(ST77XX_WHITE);

  // ===== PLAYER MOVE =====
  if(!digitalRead(btnUp)) playerY -= 2;
  if(!digitalRead(btnDown)) playerY += 2;
  if(!digitalRead(btnLeft)) playerX -= 2;
  if(!digitalRead(btnRight)) playerX += 2;

  // Giới hạn màn hình
  if(playerX < 0) playerX = 0;
  if(playerX > 150) playerX = 150;
  if(playerY < 0) playerY = 0;
  if(playerY > 118) playerY = 118;

  // ===== BOSS AI =====
  float dx = playerX - bossX;
  float dy = playerY - bossY;
  float dist = sqrt(dx*dx + dy*dy);

  if(dist > 1) {
    bossX += dx / dist * 0.7;
    bossY += dy / dist * 0.7;
  }

  // ===== HƯỚNG ĐÁNH =====
  if(dist > 0) {
    attackDirX = dx / dist;
    attackDirY = dy / dist;
  }

  // ===== ATTACK =====
  if(!digitalRead(btnAtk) && !attacking) {
    attacking = true;
    attackTimer = 10;
  }

  int punchX = playerX + attackDirX * 15;
  int punchY = playerY + attackDirY * 15;

  if(attacking) {

    tft.fillRect(punchX, punchY, 8, 8, ST77XX_BLACK);
    attackTimer--;

    if(attackTimer <= 0)
      attacking = false;

    if(abs(punchX - bossX) < 12 && abs(punchY - bossY) < 12) {
      bossHP--;
      delay(60);
    }
  }

  // ===== DRAW PLAYER =====
  tft.fillRect(playerX, playerY, 10, 10, ST77XX_BLUE);

  // ===== DRAW BOSS =====
  if(bossAlive)
    tft.fillRect(bossX, bossY, 14, 14, ST77XX_RED);

  drawBossHP();

  // ===== WIN =====
  if(bossHP <= 0) {
    bossAlive = false;
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(40, 60);
    tft.print("YOU WIN");
    while(1);
  }

  delay(20);
}
