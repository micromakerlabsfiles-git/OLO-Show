#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// 📺 SCREEN SELECTION (Configured by compile / Python tool)
// ==========================================
// Select one of the screen options:
//////////////#define USE_SSD1306
#define USE_SH1106

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// ==========================================
// 🔌 HARDWARE PIN DEFINITIONS
// ==========================================
#define I2C_SDA_PIN   20
#define I2C_SCL_PIN   21
#define TOUCH_PIN      1
#define BUZZER_PIN     2
#define LED_PIN        6

// ==========================================
// 🌈 LED CONFIGURATION
// ==========================================
#define NUM_PIXELS     1       // Number of NeoPixels (set to 1 for onboard, increase for strips)
#define LED_BRIGHTNESS 60      // LED brightness scale (0 to 255)

// ==========================================
// 👆 TOUCH SENSOR CONFIGURATION
// ==========================================
#define TOUCH_ACTIVE_LEVEL HIGH
#define DEBOUNCE_TIME      50   // Debounce delay in milliseconds
#define LONG_PRESS_TIME    1000 // Press duration in milliseconds for animation reset

// ==========================================
// 🔊 SOUND CONFIGURATION
// ==========================================
#define SOUND_ENABLED      true

#endif // CONFIG_H
