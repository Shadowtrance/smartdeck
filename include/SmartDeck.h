#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>

#include <TJpg_Decoder.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include <vector>
#include <string>
#include <set>
#include <math.h>

// --- NEW CONTROL FLAG ---
// 0: Write directly to USB Serial Port
// 1: Send via ESP-NOW to dongle
#define DONGLE_MODE 0

// NEW: Receiver MAC address for dongle mode (required if DONGLE_MODE = 1)
// Note: You must enter the MAC address here as 6 bytes.
extern uint8_t receiver_mac[];

#if DONGLE_MODE == 1
#include <WiFi.h> // Required for ESP-NOW
#include <esp_now.h>
#endif

#include "DrawHelper.h"
#include "ScreenHelper.h"
#include "SdUpload.h"
#include "Comms.h"

#if defined(JC3248W535C)
extern Arduino_Canvas *gfx;
#elif defined(RGB_PANEL)
extern Arduino_RGB_Display *gfx;
#else
extern Arduino_GFX *gfx;
#endif

/* Shared Preferences */
#include <Preferences.h>
extern Preferences preferences;

#ifndef JC3248W535C
#include <bb_captouch.h>
extern BBCapTouch bbct;
#else
#include <AXS15231B_Touch.h>
extern AXS15231B_Touch touch;
#endif

/* Basic configuration constants */
//2.8 inch / 3.5 inch
#ifdef SMALL
#define CELL_W 80
#define CELL_H 80
#define CELL_PADDING 10
#define STROKE_WIDTH 2
#define CORNER_RADIUS 20
#define TITLE_BOX_HEIGHT 15
#define TITLE_BOX_MARGIN_Y 2
#endif

//4.3 inch / 5 inch
#ifdef MEDIUM
#define CELL_W 90
#define CELL_H 90
#define CELL_PADDING 17
#define STROKE_WIDTH 2
#define CORNER_RADIUS 15
#define TITLE_BOX_HEIGHT 35
#define TITLE_BOX_MARGIN_Y 3
#endif

//7 inch
#ifdef LARGE
#define CELL_W 110
#define CELL_H 110
#define CELL_PADDING 17
#define STROKE_WIDTH 2
#define CORNER_RADIUS 20
#define TITLE_BOX_HEIGHT 35
#define TITLE_BOX_MARGIN_Y 3
#endif

/* Backlight PWM defaults */
#define BL_PWM_CHANNEL 0
#define BL_PWM_FREQ 1500
#define BL_PWM_RESOLUTION 8

/* Device name */
#define DEVICE_NAME "Smart Deck"

/* Shared globals used across modules */
extern int current_brightness;
extern bool sleep_enabled;
extern unsigned long sleep_timeout_ms;
extern unsigned long last_activity_time;
extern bool is_sleeping;

extern String serial_cmd_buffer;
extern bool serial_cmd_ready;

extern JsonDocument doc;
extern uint8_t current_page;

extern uint32_t screenWidth;
extern uint32_t screenHeight;

extern uint16_t theme_bg_color_rgb565;
extern uint16_t theme_btn_color_rgb565;
extern uint16_t theme_stroke_color_rgb565;
extern uint16_t theme_empty_btn_color_rgb565;
extern uint16_t theme_click_stroke_color_rgb565;
extern uint16_t theme_text_color_rgb565;
extern uint16_t theme_shadow_color_rgb565;

struct ButtonInfo {
  int x;
  int y;
  int w;
  int h;
  String action;
  String value;
  bool defined = false;
};

enum TimerState {
  TIMER_INACTIVE,
  TIMER_RUNNING,
  TIMER_FINISHED
};

struct TimerInfo {
  TimerState state = TIMER_INACTIVE;
  int duration = 0;
  unsigned long startTime = 0;
  int lastSeconds = -1;
};

struct CounterInfo {
  long currentValue = 0;
  long startValue = 0;
  String action = "increment";
};

struct ToggleInfo {
  bool state = false;
  uint16_t onColor = 0x07E0;
};

struct JpegDrawInfo {
  int x;
  int y;
  int maxWidth;
  int maxHeight;
  int x_offset;
  int y_offset;
  int radius;
};

extern JpegDrawInfo jpegInfo;

/* Shared containers */
extern std::vector<ButtonInfo> current_buttons;
extern std::vector<TimerInfo> current_timers;
extern std::vector<CounterInfo> current_counters;
extern std::vector<ToggleInfo> current_toggles;

/* Touch state helpers */
extern bool was_touched;
extern int last_touched_button_index;
extern unsigned long touch_start_time;
extern bool long_press_triggered;
