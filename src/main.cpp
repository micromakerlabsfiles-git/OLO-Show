#include <Arduino.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>

#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#include "Config.h"
#include "GeneratedVideos.h"

// Preferences for non-volatile flash storage
Preferences prefs;

// Dynamic pin allocations (defaulting to Config.h values)
int sda_pin = I2C_SDA_PIN;
int scl_pin = I2C_SCL_PIN;
int touch_pin = TOUCH_PIN;
int buzzer_pin = BUZZER_PIN;
int led_pin = LED_PIN;

// Bootloader parameters
int boot_delay = 1;           // Boot screen delay in seconds
int active_slot = 1;          // Active animation slot
int boot_melody = 1;          // Selected startup tone chime
char splash_text[32] = "Video on OLED Display";

// Runtime controller settings
int led_brightness = LED_BRIGHTNESS;
bool playState = true;
int led_effect = 1;           // 1: Rainbow, 0: OFF
bool sound_enabled = SOUND_ENABLED;
int play_mode = 0;            // 0: Single Loop, 1: In Order, 2: Random
int gap_delay_ms = 1000;      // Delay between plays in milliseconds (default 1 s)

// Playback gap state variables
unsigned long gapStartMillis = 0;
bool inGap = false;

// Triple-tap clock popup state
unsigned long clockPopupMillis = 0;  // 0 = not in popup
int  preClockSlot = 1;               // slot to return to after popup

// Screen hardware dynamic instances
#include <Adafruit_SSD1306.h>
#include <Adafruit_SH110X.h>

Adafruit_SSD1306* display_ssd = nullptr;
Adafruit_SH1106G* display_sh = nullptr;
int display_type = 1;         // 1: SSD1306, 2: SH1106

// NeoPixel LED pointer
Adafruit_NeoPixel* pixels = nullptr;
uint32_t firstPixelHue = 0;
unsigned long lastPixelUpdate = 0;

// Frame state variables
int currentFrame = 0;
unsigned long previousFrameMillis = 0;

// Touch button double click state machine
bool lastButtonState = !TOUCH_ACTIVE_LEVEL;
bool buttonState = !TOUCH_ACTIVE_LEVEL;
unsigned long buttonPressedTime = 0;
bool isLongPressHandled = false;
unsigned long lastTouchDebounceTime = 0;

int clickCount = 0;
unsigned long lastReleaseTime = 0;
const unsigned long DOUBLE_CLICK_TIMEOUT = 280; // milliseconds

// Time synchronizations variables
int clock_hour = 12;
int clock_minute = 0;
int clock_second = 0;
int clock_day = 1;
int clock_month = 1;
int clock_year = 2026;
unsigned long lastClockTick = 0;
unsigned long last_web_activity = 0;
bool web_connected = false;
bool clock_12h_mode = false;  // false: 24h, true: 12h with AM/PM

#define CLOCK_SLOT      (MAX_ANIM_SLOTS + 1)

bool slotHasFrames(int slot) {
  if (slot < 1 || slot > MAX_ANIM_SLOTS) return false;
  return (SLOT_TABLE[slot - 1].total_frames > 0);
}

void advanceSlot() {
  // Never auto-advance into CLOCK_SLOT — clock is only shown via triple-tap
  int next = active_slot;
  for (int tries = 0; tries <= MAX_ANIM_SLOTS; tries++) {
    next++;
    if (next > MAX_ANIM_SLOTS) next = 1;
    if (slotHasFrames(next)) break;
  }
  active_slot = next;
  currentFrame = 0;
  previousFrameMillis = 0;
  prefs.begin("oloshow", false);
  prefs.putInt("act_slot", active_slot);
  prefs.end();
}

void selectRandomSlot() {
  int active_slots[MAX_ANIM_SLOTS];
  int count = 0;
  for (int i = 1; i <= MAX_ANIM_SLOTS; i++) {
    if (slotHasFrames(i)) {
      active_slots[count++] = i;
    }
  }
  if (count > 0) {
    int rnd = random(count);
    active_slot = active_slots[rnd];
  }
  currentFrame = 0;
  previousFrameMillis = 0;
  prefs.begin("oloshow", false);
  prefs.putInt("act_slot", active_slot);
  prefs.end();
}

// ==========================================
// 🔊 SOUND MODULE
// ==========================================
void playBeep(unsigned int frequency, unsigned long duration) {
  if (sound_enabled) {
    tone(buzzer_pin, frequency, duration);
  }
}

void playStartupMelody() {
  if (boot_melody == 1) { // Standard Chime
    playBeep(880, 100);
    delay(100);
    playBeep(1760, 200);
  } else if (boot_melody == 2) { // Fanfare
    playBeep(523, 100); delay(120);
    playBeep(659, 100); delay(120);
    playBeep(784, 100); delay(120);
    playBeep(1047, 250);
  }
}

// ==========================================
// 📺 UNIFIED SCREEN INTERFACE
// ==========================================
void clearDisplay() {
  if (display_type == 1 && display_ssd) display_ssd->clearDisplay();
  else if (display_type == 2 && display_sh) display_sh->clearDisplay();
}

void showDisplay() {
  if (display_type == 1 && display_ssd) display_ssd->display();
  else if (display_type == 2 && display_sh) display_sh->display();
}

void setFont(const GFXfont* f) {
  if (display_type == 1 && display_ssd) display_ssd->setFont(f);
  else if (display_type == 2 && display_sh) display_sh->setFont(f);
}

void setTextSize(uint8_t s) {
  if (display_type == 1 && display_ssd) display_ssd->setTextSize(s);
  else if (display_type == 2 && display_sh) display_sh->setTextSize(s);
}

void setTextColor(uint16_t c) {
  if (display_type == 1 && display_ssd) display_ssd->setTextColor(c);
  else if (display_type == 2 && display_sh) display_sh->setTextColor(c);
}

void setCursor(int16_t x, int16_t y) {
  if (display_type == 1 && display_ssd) display_ssd->setCursor(x, y);
  else if (display_type == 2 && display_sh) display_sh->setCursor(x, y);
}

void printStr(const char* s) {
  if (display_type == 1 && display_ssd) display_ssd->print(s);
  else if (display_type == 2 && display_sh) display_sh->print(s);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (display_type == 1 && display_ssd) display_ssd->drawRect(x, y, w, h, c);
  else if (display_type == 2 && display_sh) display_sh->drawRect(x, y, w, h, c);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (display_type == 1 && display_ssd) display_ssd->fillRect(x, y, w, h, c);
  else if (display_type == 2 && display_sh) display_sh->fillRect(x, y, w, h, c);
}

void drawPixelD(int16_t x, int16_t y, uint16_t c) {
  if (display_type == 1 && display_ssd) display_ssd->drawPixel(x, y, c);
  else if (display_type == 2 && display_sh) display_sh->drawPixel(x, y, c);
}

int16_t getTextBoundsX(const char* s, int16_t x, int16_t y) {
  int16_t x1, y1; uint16_t w, h;
  if (display_type == 1 && display_ssd)
    display_ssd->getTextBounds(s, x, y, &x1, &y1, &w, &h);
  else if (display_type == 2 && display_sh)
    display_sh->getTextBounds(s, x, y, &x1, &y1, &w, &h);
  else return x;
  return x1;
}

uint16_t getTextBoundsW(const char* s, int16_t x, int16_t y) {
  int16_t x1, y1; uint16_t w = 0, h = 0;
  if (display_type == 1 && display_ssd)
    display_ssd->getTextBounds(s, x, y, &x1, &y1, &w, &h);
  else if (display_type == 2 && display_sh)
    display_sh->getTextBounds(s, x, y, &x1, &y1, &w, &h);
  return w;
}

void printCentered(const char* s, int16_t y) {
  int16_t x1, y1; uint16_t w, h;
  if (display_type == 1 && display_ssd)
    display_ssd->getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  else if (display_type == 2 && display_sh)
    display_sh->getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  else { w = strlen(s) * 6; }
  setCursor((SCREEN_WIDTH - w) / 2, y);
  printStr(s);
}

void drawBitmapFrame(const uint8_t* bmp) {
  if (display_type == 1 && display_ssd)
    display_ssd->drawBitmap(0, 0, bmp, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
  else if (display_type == 2 && display_sh)
    display_sh->drawBitmap(0, 0, bmp, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
}

void drawText1(int x, int y, const String& s) {
  setFont(nullptr);
  setTextSize(1);
  setTextColor(WHITE);
  setCursor(x, y);
  printStr(s.c_str());
}

// ==========================================
// ⚙️ CONFIG LOADER & PERSISTENCE
// ==========================================
void loadConfigurations() {
  prefs.begin("oloshow", true);
  sda_pin = prefs.getInt("sda", I2C_SDA_PIN);
  scl_pin = prefs.getInt("scl", I2C_SCL_PIN);
  touch_pin = prefs.getInt("touch", TOUCH_PIN);
  buzzer_pin = prefs.getInt("buzzer", I2C_SCL_PIN == 21 && TOUCH_PIN == 1 ? 2 : BUZZER_PIN);
  led_pin = prefs.getInt("led", LED_PIN);
  display_type = prefs.getInt("disp_type", 1);
  boot_delay = prefs.getInt("boot_del", 1);
  active_slot = prefs.getInt("act_slot", 1);
  boot_melody = prefs.getInt("melody", 1);
  led_brightness = prefs.getInt("led_bright", LED_BRIGHTNESS);
  led_effect = prefs.getInt("led_eff", 1);
  sound_enabled = prefs.getBool("sound_en", SOUND_ENABLED);
  clock_12h_mode = prefs.getBool("clk_12h", false);
  play_mode = prefs.getInt("play_mode", 0);
  gap_delay_ms = prefs.getInt("gap_delay", 1000);
  String splash = prefs.getString("splash", "Video on OLED Display");
  strncpy(splash_text, splash.c_str(), sizeof(splash_text) - 1);
  splash_text[sizeof(splash_text) - 1] = '\0';
  prefs.end();
}

// ==========================================
// 🌈 NeoPixel LED UPDATE
// ==========================================
void updateLED() {
  if (!pixels) return;
  if (led_effect == 0) {
    pixels->clear();
    pixels->show();
    return;
  }
  if (led_effect == 1) { // Rainbow Cycle
    if (millis() - lastPixelUpdate >= 20) {
      lastPixelUpdate = millis();
      pixels->setBrightness(led_brightness);
      for (int i = 0; i < NUM_PIXELS; i++) {
        int pixelHue = firstPixelHue + (i * 65536L / NUM_PIXELS);
        pixels->setPixelColor(i, pixels->gamma32(pixels->ColorHSV(pixelHue)));
      }
      pixels->show();
      firstPixelHue += 256;
    }
  }
  else if (led_effect == 2) { // Solid Red
    pixels->setBrightness(led_brightness);
    pixels->fill(pixels->Color(255, 0, 0));
    pixels->show();
  }
  else if (led_effect == 3) { // Solid Green
    pixels->setBrightness(led_brightness);
    pixels->fill(pixels->Color(0, 255, 0));
    pixels->show();
  }
  else if (led_effect == 4) { // Solid Blue
    pixels->setBrightness(led_brightness);
    pixels->fill(pixels->Color(0, 0, 255));
    pixels->show();
  }
  else if (led_effect == 5) { // Solid Cyan
    pixels->setBrightness(led_brightness);
    pixels->fill(pixels->Color(0, 255, 255));
    pixels->show();
  }
  else if (led_effect == 6) { // Solid Magenta
    pixels->setBrightness(led_brightness);
    pixels->fill(pixels->Color(255, 0, 255));
    pixels->show();
  }
  else if (led_effect == 7) { // Solid Yellow
    pixels->setBrightness(led_brightness);
    pixels->fill(pixels->Color(255, 255, 0));
    pixels->show();
  }
  else if (led_effect == 8) { // Color Breath
    if (millis() - lastPixelUpdate >= 20) {
      lastPixelUpdate = millis();
      pixels->setBrightness(led_brightness);
      float intensity = (sin(millis() / 1000.0) + 1.0) / 2.0;
      uint32_t breathColor = pixels->ColorHSV(firstPixelHue, 255, intensity * 255);
      pixels->fill(breathColor);
      pixels->show();
      firstPixelHue += 128;
    }
  }
  else if (led_effect == 9) { // Fire Flicker
    if (millis() - lastPixelUpdate >= random(50, 150)) {
      lastPixelUpdate = millis();
      pixels->setBrightness(led_brightness);
      for (int i = 0; i < NUM_PIXELS; i++) {
        int r = random(120, 255);
        int g = random(20, 70);
        pixels->setPixelColor(i, pixels->Color(r, g, 0));
      }
      pixels->show();
    }
  }
  else if (led_effect == 10) { // LED Chase
    static int chasePos = 0;
    static int chaseDir = 1;
    if (millis() - lastPixelUpdate >= 100) {
      lastPixelUpdate = millis();
      pixels->setBrightness(led_brightness);
      pixels->clear();
      pixels->setPixelColor(chasePos, pixels->Color(255, 100, 0));
      if (NUM_PIXELS > 1) {
        if (chasePos > 0) pixels->setPixelColor(chasePos - 1, pixels->Color(100, 40, 0));
        if (chasePos < NUM_PIXELS - 1) pixels->setPixelColor(chasePos + 1, pixels->Color(100, 40, 0));
      }
      pixels->show();
      chasePos += chaseDir;
      if (chasePos >= NUM_PIXELS || chasePos < 0) {
        chaseDir = -chaseDir;
        chasePos += 2 * chaseDir;
        if (chasePos >= NUM_PIXELS) chasePos = NUM_PIXELS - 1;
        if (chasePos < 0) chasePos = 0;
      }
    }
  }
  else if (led_effect == 11) { // Starry Sparkle
    if (millis() - lastPixelUpdate >= 80) {
      lastPixelUpdate = millis();
      pixels->setBrightness(led_brightness);
      for (int i = 0; i < NUM_PIXELS; i++) {
        pixels->setPixelColor(i, pixels->Color(0, 0, 20));
      }
      int p1 = random(NUM_PIXELS);
      pixels->setPixelColor(p1, pixels->Color(255, 255, 255));
      if (random(10) > 5) {
        int p2 = random(NUM_PIXELS);
        pixels->setPixelColor(p2, pixels->Color(200, 200, 255));
      }
      pixels->show();
    }
  }
}

// ==========================================
// 👆 TOUCH BUTTON STATE MACHINE
// ==========================================
void handleTouchButton() {
  bool reading = digitalRead(touch_pin);

  if (reading != lastButtonState) {
    lastTouchDebounceTime = millis();
  }

  if ((millis() - lastTouchDebounceTime) > DEBOUNCE_TIME) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == TOUCH_ACTIVE_LEVEL) {
        buttonPressedTime = millis();
        isLongPressHandled = false;
      } else {
        if (!isLongPressHandled) {
          unsigned long pressDuration = millis() - buttonPressedTime;
          if (pressDuration > 50) {
            if (pressDuration < LONG_PRESS_TIME) {
              clickCount++;
              lastReleaseTime = millis();
            }
          }
        }
      }
    }
  }

  if (clickCount > 0 && (millis() - lastReleaseTime) > DOUBLE_CLICK_TIMEOUT) {
    if (clickCount == 1) {
      playState = !playState;
      playBeep(1000, 80);
      Serial.print("[Touch] Single Click. PlayState: ");
      Serial.println(playState ? "PLAYING" : "PAUSED");
    } else if (clickCount == 2) {
      int prev = active_slot;
      advanceSlot();
      playBeep(1200, 80);
      delay(80);
      playBeep(1600, 120);
      Serial.printf("[Touch] Double tap. Slot %d -> %d\n", prev, active_slot);
    } else if (clickCount >= 3) {
      // Triple-tap: show clock for 5 seconds then return
      if (active_slot != CLOCK_SLOT) {
        preClockSlot = active_slot;        // remember where to return
      }
      active_slot = CLOCK_SLOT;
      clockPopupMillis = millis();          // start 5-second countdown
      currentFrame = 0;
      previousFrameMillis = 0;
      playBeep(1800, 60); delay(70);
      playBeep(1800, 60); delay(70);
      playBeep(2200, 120);
      Serial.println("[Touch] Triple tap -> Clock popup for 5 s.");
    }
    clickCount = 0;
  }

  if (buttonState == TOUCH_ACTIVE_LEVEL && !isLongPressHandled) {
    if (millis() - buttonPressedTime >= LONG_PRESS_TIME) {
      currentFrame = 0;
      playState = true;
      isLongPressHandled = true;
      clickCount = 0;
      playBeep(1500, 300);
      Serial.println("[Touch] Long press -> Reset animation.");
    }
  }

  lastButtonState = reading;
}

// ==========================================
// ⏰ CLOCK ENGINE
// ==========================================
void updateClock() {
  if (millis() - lastClockTick >= 1000) {
    lastClockTick = millis();
    clock_second++;
    if (clock_second >= 60) {
      clock_second = 0;
      clock_minute++;
      if (clock_minute >= 60) {
        clock_minute = 0;
        clock_hour++;
        if (clock_hour >= 24) {
          clock_hour = 0;
          clock_day++;
        }
      }
    }
  }

  // Heartbeat check for Web Serial Dashboard connection
  if (web_connected && (millis() - last_web_activity > 5000)) {
    web_connected = false;
    Serial.println("[WebSerial] Connection lost.");
  }
}

// ── Beautiful minimalistic clock display screen ───────────────────────────
void renderClockScreen() {
  clearDisplay();

  // Draw double borders
  drawRect(0, 0, 128, 64, WHITE);
  drawRect(2, 2, 124, 60, WHITE);

  // Time format
  int disp_hour = clock_hour;
  bool is_pm = (disp_hour >= 12);
  if (clock_12h_mode) {
    disp_hour = disp_hour % 12;
    if (disp_hour == 0) disp_hour = 12;
  }

  char timeBuf[12];
  sprintf(timeBuf, "%02d:%02d", disp_hour, clock_minute);

  // 1. Large Time Hour:Minute — FreeSansBold24pt7b (~35px height)
  setFont(&FreeSansBold24pt7b);
  setTextColor(WHITE);
  setTextSize(1);
  
  uint16_t wTime = getTextBoundsW(timeBuf, 0, 38);
  int16_t xTime = (128 - wTime) / 2;
  
  // Shift slightly left to accommodate the seconds cleanly
  setCursor(xTime - 8, 38);
  printStr(timeBuf);

  // 2. Seconds (Tiny default system font, top-right of the time)
  setFont(nullptr);
  setTextSize(1);
  setCursor(xTime + wTime - 6, 15);
  char secBuf[8];
  sprintf(secBuf, "%02d", clock_second);
  printStr(secBuf);

  // 3. AM/PM Indicator (Tiny default system font, below seconds)
  if (clock_12h_mode) {
    setCursor(xTime + wTime - 6, 25);
    printStr(is_pm ? "PM" : "AM");
  }

  // 4. Date (Tiny default system font, centered at bottom)
  static const char* MONTHS[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
  };
  char dateBuf[24];
  int m = clock_month < 1 ? 1 : (clock_month > 12 ? 12 : clock_month);
  sprintf(dateBuf, "%s %02d, %04d", MONTHS[m - 1], clock_day, clock_year);
  
  setCursor((128 - (strlen(dateBuf) * 6)) / 2, 53);
  printStr(dateBuf);

  showDisplay();
}

// ============================================================
// 📡  SERIAL COMMAND PROCESSOR
// ============================================================
void handleSerialCommands() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "GET_STATUS") {
    web_connected      = true;
    last_web_activity  = millis();
    Serial.print("{\"status\":\"OK\"");
    Serial.printf(",\"sda\":%d",         sda_pin);
    Serial.printf(",\"scl\":%d",         scl_pin);
    Serial.printf(",\"touch\":%d",       touch_pin);
    Serial.printf(",\"buzzer\":%d",      buzzer_pin);
    Serial.printf(",\"led\":%d",         led_pin);
    Serial.printf(",\"display_type\":%d",display_type);
    Serial.printf(",\"boot_delay\":%d",  boot_delay);
    Serial.printf(",\"active_slot\":%d", active_slot);
    Serial.printf(",\"melody\":%d",      boot_melody);
    Serial.printf(",\"splash\":\"%s\"",  splash_text);
    Serial.printf(",\"brightness\":%d",  led_brightness);
    Serial.printf(",\"play\":%d",        playState ? 1 : 0);
    Serial.printf(",\"led_effect\":%d",  led_effect);
    Serial.printf(",\"sound_state\":%d", sound_enabled ? 1 : 0);
    Serial.printf(",\"clock_12h\":%d",   clock_12h_mode ? 1 : 0);
    Serial.printf(",\"play_mode\":%d",   play_mode);
    Serial.printf(",\"gap_delay\":%d",    gap_delay_ms);
    Serial.printf(",\"max_slots\":%d",   MAX_ANIM_SLOTS);
    Serial.println("}");
  }
  else if (cmd.startsWith("SET_TIME:")) {
    // Format: SET_TIME:hh,mm,ss[,day,month,year]
    web_connected = true; last_web_activity = millis();
    String d = cmd.substring(9);
    int c1=d.indexOf(','), c2=d.indexOf(',',c1+1);
    if (c1!=-1 && c2!=-1) {
      clock_hour   = d.substring(0,c1).toInt();
      clock_minute = d.substring(c1+1,c2).toInt();
      clock_second = d.substring(c2+1).toInt();
      // Optional date
      int c3=d.indexOf(',',c2+1);
      if (c3!=-1) {
        int c4=d.indexOf(',',c3+1);
        if (c4!=-1) {
          clock_day   = d.substring(c3+1,c4).toInt();
          int c5=d.indexOf(',',c4+1);
          if (c5!=-1) {
            clock_month = d.substring(c4+1,c5).toInt();
            clock_year  = d.substring(c5+1).toInt();
          }
        }
      }
      lastClockTick = millis();
      Serial.println("{\"status\":\"TIME_SYNCED\"}");
    }
  }
  else if (cmd == "SET_CLOCK_12H") {
    clock_12h_mode = true;
    prefs.begin("oloshow",false); prefs.putBool("clk_12h",true); prefs.end();
    Serial.println("{\"status\":\"CLOCK_12H\"}");
  }
  else if (cmd == "SET_CLOCK_24H") {
    clock_12h_mode = false;
    prefs.begin("oloshow",false); prefs.putBool("clk_12h",false); prefs.end();
    Serial.println("{\"status\":\"CLOCK_24H\"}");
  }
  else if (cmd.startsWith("SET_PINS:")) {
    String d = cmd.substring(9);
    int c1=d.indexOf(','),c2=d.indexOf(',',c1+1),
        c3=d.indexOf(',',c2+1),c4=d.indexOf(',',c3+1),c5=d.indexOf(',',c4+1);
    if (c1!=-1&&c2!=-1&&c3!=-1&&c4!=-1&&c5!=-1) {
      prefs.begin("oloshow",false);
      prefs.putInt("sda",   d.substring(0,c1).toInt());
      prefs.putInt("scl",   d.substring(c1+1,c2).toInt());
      prefs.putInt("touch", d.substring(c2+1,c3).toInt());
      prefs.putInt("buzzer",d.substring(c3+1,c4).toInt());
      prefs.putInt("led",   d.substring(c4+1,c5).toInt());
      prefs.putInt("disp_type",d.substring(c5+1).toInt());
      prefs.end();
      Serial.println("{\"status\":\"PINS_SAVED\"}");
      delay(400); ESP.restart();
    }
  }
  else if (cmd.startsWith("SET_BOOTLOADER:")) {
    String d = cmd.substring(15);
    int c1=d.indexOf(','),c2=d.indexOf(',',c1+1),c3=d.indexOf(',',c2+1);
    if (c1!=-1&&c2!=-1&&c3!=-1) {
      int n_del=d.substring(0,c1).toInt(), n_slot=d.substring(c1+1,c2).toInt(),
          n_mel=d.substring(c2+1,c3).toInt();
      String n_spl=d.substring(c3+1); n_spl.trim();
      prefs.begin("oloshow",false);
      prefs.putInt("boot_del",n_del); prefs.putInt("act_slot",n_slot);
      prefs.putInt("melody",n_mel);   prefs.putString("splash",n_spl);
      prefs.end();
      boot_delay=n_del; active_slot=n_slot; boot_melody=n_mel;
      currentFrame = 0;
      previousFrameMillis = 0;
      strncpy(splash_text,n_spl.c_str(),sizeof(splash_text)-1);
      Serial.println("{\"status\":\"BOOTLOADER_SAVED\"}");
    }
  }
  else if (cmd.startsWith("SET_CTRL:")) {
    String d = cmd.substring(9);
    int c1=d.indexOf(','),c2=d.indexOf(',',c1+1),c3=d.indexOf(',',c2+1),c4=d.indexOf(',',c3+1),c5=d.indexOf(',',c4+1);
    if (c1!=-1&&c2!=-1&&c3!=-1&&c4!=-1) {
      led_brightness = d.substring(0,c1).toInt();
      playState      = d.substring(c1+1,c2).toInt() == 1;
      led_effect     = d.substring(c2+1,c3).toInt();
      sound_enabled  = d.substring(c3+1,c4).toInt() == 1;
      play_mode      = d.substring(c4+1, c5!=-1 ? c5 : d.length()).toInt();
      if (c5 != -1) {
        int gd = d.substring(c5+1).toInt();
        if (gd >= 0 && gd <= 10000) gap_delay_ms = gd;
      }
      prefs.begin("oloshow",false);
      prefs.putInt("led_bright",led_brightness);
      prefs.putInt("led_eff",led_effect);
      prefs.putBool("sound_en",sound_enabled);
      prefs.putInt("play_mode",play_mode);
      prefs.putInt("gap_delay",gap_delay_ms);
      prefs.end();
      Serial.println("{\"status\":\"CTRL_UPDATED\"}");
    }
  }
}

// ============================================================
// 🚀  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  loadConfigurations();

  pinMode(touch_pin, INPUT);
  pinMode(buzzer_pin, OUTPUT);
  digitalWrite(buzzer_pin, LOW);

  pixels = new Adafruit_NeoPixel(NUM_PIXELS, led_pin, NEO_GRB + NEO_KHZ800);
  pixels->begin(); pixels->clear(); pixels->show();

  // I2C init
#if defined(ARDUINO_ARCH_RP2040)
  Wire.setSDA(sda_pin); Wire.setSCL(scl_pin); Wire.begin();
#elif defined(ESP32)
  Wire.begin(sda_pin, scl_pin);
#else
  Wire.begin();
#endif
  Wire.setClock(800000);

  // Display init
  if (display_type == 1) {
    display_ssd = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    if (!display_ssd->begin(SSD1306_SWITCHCAPVCC, 0x3C))
      Serial.println("[Display] SSD1306 init failed!");
  } else {
    display_sh = new Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    if (!display_sh->begin(0x3C, true))
      Serial.println("[Display] SH1106 init failed!");
  }

  // Splash screen with fancy font
  clearDisplay();
  drawRect(0,0,128,64,WHITE);
  drawRect(2,2,124,60,WHITE);

  setFont(&FreeSansBold9pt7b);
  setTextColor(WHITE); setTextSize(1);
  printCentered("OLO SHOW", 22);

  setFont(nullptr); setTextColor(WHITE); setTextSize(1);
  printCentered(splash_text, 36);

  char pinInfo[24];
  printCentered("By Micromaker Labs", 52);

  showDisplay();
  playStartupMelody();
  randomSeed(micros());
  delay(boot_delay * 1000);

  // If configured slot is empty, find the first non-empty one
  if (active_slot != CLOCK_SLOT && !slotHasFrames(active_slot)) {
    advanceSlot();
  }
}

// ============================================================
// 🔄  LOOP
// ============================================================
void loop() {
  handleSerialCommands();
  handleTouchButton();
  updateLED();
  updateClock();

  unsigned long now = millis();

  // ── Clock slot ────────────────────────────────────────────────────────────
  if (active_slot == CLOCK_SLOT) {
    // Auto-return after 5-second popup (clockPopupMillis == 0 means permanent/web-set)
    if (clockPopupMillis != 0 && (now - clockPopupMillis >= 5000)) {
      clockPopupMillis = 0;
      active_slot = preClockSlot;
      if (!slotHasFrames(active_slot)) advanceSlot();
      currentFrame = 0;
      previousFrameMillis = 0;
      inGap = false;
      Serial.println("[Clock] Popup expired -> returning to animation.");
      return;
    }
    if (now - previousFrameMillis >= 250) {
      previousFrameMillis = now;
      renderClockScreen();
    }
    return;
  }

  // ── Animation slot ────────────────────────────────────────────────────────
  int si = active_slot - 1;    // 0-based index into SLOT_TABLE
  if (si < 0 || si >= MAX_ANIM_SLOTS) si = 0;

  const SlotInfo& sl = SLOT_TABLE[si];

  if (sl.total_frames == 0) return;  // empty slot — wait for double-tap

  if (playState) {
    if (inGap) {
      if (now - gapStartMillis >= (unsigned long)gap_delay_ms) {
        inGap = false;
        previousFrameMillis = now;
        
        // Transition according to play_mode
        if (play_mode == 1) { // Play one by one in order
          advanceSlot();
        } else if (play_mode == 2) { // Random video play after one
          selectRandomSlot();
        } else {
          // Single file loop: reset frame
          currentFrame = 0;
        }
      }
    } else {
      if (now - previousFrameMillis >= (unsigned long)sl.frame_delay) {
        previousFrameMillis = now;
        clearDisplay();
        drawBitmapFrame(sl.frames[currentFrame]);
        showDisplay();
        currentFrame++;
        if (currentFrame >= sl.total_frames) {
          inGap = true;
          gapStartMillis = now;
          clearDisplay();
          showDisplay();
        }
      }
    }
  } else {
    // Paused — draw current frame + small indicator dot in bottom-right corner
    if (now - previousFrameMillis >= 500) {
      previousFrameMillis = now;
      clearDisplay();
      drawBitmapFrame(sl.frames[currentFrame >= sl.total_frames ? 0 : currentFrame]);
      // 3×3 filled dot at bottom-right
      fillRect(123, 59, 3, 3, WHITE);
      showDisplay();
    }
  }
}
