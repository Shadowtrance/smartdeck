/* * OZAN'S WEB DECK - v4.0 (Serial and ESP-NOW Mode)
 * * This version does not include Wi-Fi and BLE. Communication is done via USB Serial Port
 * or ESP-NOW (Dongle Mode).
 * */

#include <Arduino.h>

#include "SmartDeck.h"
#include "DisplayConfig.h"

uint8_t receiver_mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

#ifndef JC3248W535C
BBCapTouch bbct;
#else
AXS15231B_Touch touch;
#endif

#ifdef ESP32
#undef F
#define F(s) (s)
#endif

String serial_cmd_buffer = "";
bool serial_cmd_ready = false;

Preferences preferences;

// Brightness / Sleep defaults
int current_brightness = 100;
bool sleep_enabled = false;
unsigned long sleep_timeout_ms = 5 * 60 * 1000;
unsigned long last_activity_time = 0;
bool is_sleeping = false;

JsonDocument doc;
uint8_t current_page = 0;

uint16_t theme_bg_color_rgb565;
uint16_t theme_btn_color_rgb565;
uint16_t theme_stroke_color_rgb565 = 0x8410;
uint16_t theme_empty_btn_color_rgb565 = 0x3186;
uint16_t theme_click_stroke_color_rgb565 = 0x05BF;
uint16_t theme_text_color_rgb565;
uint16_t theme_shadow_color_rgb565;

JpegDrawInfo jpegInfo;

std::vector<ButtonInfo> current_buttons;
std::vector<TimerInfo> current_timers;
std::vector<CounterInfo> current_counters;
std::vector<ToggleInfo> current_toggles;

bool was_touched = false;
int last_touched_button_index = -1;
unsigned long touch_start_time = 0;
bool long_press_triggered = false;

uint32_t screenWidth = DISPLAY_WIDTH;
uint32_t screenHeight = DISPLAY_HEIGHT;

// --- SETUP FUNCTION ---
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Setup (v4.0 - Serial/ESP-NOW Only)...");

  // --- CONDITIONAL ESP-NOW INITIALIZATION ---
#if DONGLE_MODE == 1
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    // Fill screen with error
  }
  // Add receiver
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiver_mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  }
  Serial.println("ESP-NOW Initialized.");
#endif
  // --- END ---

  delay(1000);
  // Initialize Screen
  // Display objects are initialized in display_config_boards.h
  if (!gfx || !gfx->begin()) {
    Serial.println("GFX Panel init failed!");
    while (1)
      ;
  }

  Serial.println("GFX Panel Initialized.");

  gfx->setRotation(ROTATION);

  // Load Preferences (Namespace: "deck_prefs")
  preferences.begin("deck_prefs", false); // false = read/write
  current_brightness = preferences.getInt("bright", 100);
  sleep_enabled = preferences.getBool("sleep_on", false);
  int sleep_min = preferences.getInt("sleep_min", 5);
  sleep_timeout_ms = sleep_min * 60 * 1000;

  // Backlight PWM Setup
#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  // Set up ESP32 PWM Channel
  ledcAttach(GFX_BL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
  ledcWriteChannel(BL_PWM_CHANNEL, 255);
    
  // Apply saved brightness
  set_brightness(current_brightness);
  Serial.printf("Backlight Init: %d%%\n", current_brightness);
#endif

  last_activity_time = millis(); // Start counter

  gfx->fillScreen(RGB565_BLACK);

  // Initialize Touch
#ifdef JC3248W535C
  touch.begin();
#else
  bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
  bbct.setOrientation(gfx->getRotation(), gfx->width(), gfx->height());
#endif
  Serial.println("Touch Initialized.");

  // Initialize SD Card
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    gfx->setCursor(10, 10);
    gfx->setTextColor(0xFFFF);
    gfx->print("SD Card FAILED!");
    while (1)
      ;
  } else {
    Serial.println("SD Card Initialized.");
    // Load JSON
    File file = SD.open("/esp_config.json", FILE_READ);
    bool config_ok = false;
    if (!file) {
      Serial.println("Failed to open config file!");
      gfx->setCursor(10, 10);
      gfx->setTextColor(0xFFFF);
      gfx->print("JSON File FAILED!");
    } else {
      DeserializationError error = deserializeJson(doc, file);
      file.close();
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        gfx->setCursor(10, 10);
        gfx->setTextColor(0xFFFF);
        gfx->print("JSON Parse FAILED!");
      } else {
        Serial.println("JSON Config Loaded and Parsed.");
        config_ok = true;
      }
    }

    // Set Theme Colors
    theme_bg_color_rgb565 = hex_to_rgb565(doc["theme"]["bg_color"] | "000000");
    theme_btn_color_rgb565 = hex_to_rgb565(doc["theme"]["btn_color"] | "333333");
    theme_text_color_rgb565 = hex_to_rgb565(doc["theme"]["text_color"] | "ffffff");
    theme_stroke_color_rgb565 = hex_to_rgb565(doc["theme"]["stroke_color"] | "555555");
    theme_shadow_color_rgb565 = hex_to_rgb565(doc["theme"]["shadow_color"] | "000000");

    // Do smart cleanup ONLY if config is successful
    if (config_ok) {
      cleanUnusedIcons();
    }
    
    // Draw First Page
    current_page = 0;
    draw_page(current_page);
    Serial.println("Initial page drawn. Setup done.");
  }

  Serial.println("SETUP_DONE");
}

void loop() {
  
  // --- Serial Port Command Check ---
  if (Serial.available() > 0) {
    // FIX 1: Define variable at the beginning
    String command = Serial.readStringUntil('\n');
    command.trim();

    // --- NEW: Brightness Setting ---
    if (command.startsWith("SET_BRIGHTNESS:")) {
        int val = command.substring(15).toInt();
        set_brightness(val);
        preferences.putInt("bright", val); // Save permanently
        Serial.printf("Brightness set to %d%%\n", val);
        update_activity(); // Activity occurred, reset counter
    }
    // --- NEW: Sleep Setting ---
    else if (command.startsWith("SET_SLEEP:")) {
        int mins = command.substring(10).toInt();
        if (mins > 0) {
            sleep_enabled = true;
            sleep_timeout_ms = mins * 60 * 1000;
            preferences.putBool("sleep_on", true);
            preferences.putInt("sleep_min", mins);
            Serial.printf("Sleep enabled: %d min\n", mins);
        } else {
            sleep_enabled = false;
            preferences.putBool("sleep_on", false);
            // If sleeping, wake up immediately
            update_activity();
            Serial.println("Sleep disabled");
        }
        update_activity();
    }
    // --- Existing Commands ---
    else if (command.equals("PING_DECK")) {
      Serial.print("PONG_DECK:");
      Serial.println(DEVICE_NAME);
      update_activity();
    }
    else if (command.equals("START_UPLOAD")) {
      handleUsbUpload();
      // Activity should be updated after upload finishes
      update_activity();
    }
    else if (command.equals("GET_SYNC")) {
        Serial.printf("SYNC_PAGE:%d\n", current_page);
        if (current_buttons.size() == current_toggles.size()) {
            for (int i = 0; i < current_buttons.size(); i++) {
                // Toggle State Sync
                if (current_buttons[i].defined && current_buttons[i].action == "toggle") {
                    int stateVal = current_toggles[i].state ? 1 : 0;
                    Serial.printf("SYNC_STATE:%d:%d\n", i, stateVal);
                }
                // Counter Value Sync
                else if (current_buttons[i].defined && current_buttons[i].action == "counter") {
                    long val = current_counters[i].currentValue;
                    Serial.printf("COUNTER_UPDATE:%d:%d:%ld\n", current_page, i, val);
                }
            }
        }
        update_activity();
    }
    // Unknown or any other data from PC
    else {
       update_activity();
    }
  }

  bool is_touched_now;
#ifndef JC3248W535C
  TOUCHINFO ti;
  is_touched_now = bbct.getSamples(&ti);
#else
  TouchPoint point;
  is_touched_now = touch.read(point);
#endif

  // --- NEW: Sleep and Wake Logic ---
  // 1. If there's touch, update activity
  if (is_touched_now) {
      // If device is in SLEEP mode
      if (is_sleeping) {
          update_activity(); // Wake up
          // IMPORTANT: We don't want button press right after waking up.
          // So we ignore the touch for this loop.
          is_touched_now = false;
        #ifndef JC3248W535C
          ti.count = 0;
        #else
          point.touched = false;
        #endif
      } else {
          // If awake, just extend the timer
          update_activity();
      }
  }
  
  // 2. Check if time has expired
  check_sleep_mode();
  
  int touch_x = -1, touch_y = -1;
  int current_touched_button_index = -1;

  if (is_touched_now) {
#ifndef JC3248W535C
    touch_x = ti.x[0];
    touch_y = ti.y[0];
#else
    touch_x = point.x;
    touch_y = point.y;
#endif
    for (int i = 0; i < current_buttons.size(); ++i) {
      const auto& btn = current_buttons[i];
      if (btn.defined && touch_x >= btn.x && touch_x < (btn.x + btn.w) && touch_y >= btn.y && touch_y < (btn.y + btn.h)) {
        current_touched_button_index = i;
        break;
      }
    }
  }

  int radius = CORNER_RADIUS;
  // --- 1. PRESS START ---
  if (is_touched_now && current_touched_button_index != -1 && last_touched_button_index != current_touched_button_index) {
    touch_start_time = millis();
    long_press_triggered = false; 

    if (current_timers[current_touched_button_index].state != TIMER_FINISHED) {
      if (last_touched_button_index != -1) {
        const auto& old_btn = current_buttons[last_touched_button_index];
        gfx->drawRoundRect(old_btn.x, old_btn.y, old_btn.w, old_btn.h, radius, theme_stroke_color_rgb565);
        gfx->drawRoundRect(old_btn.x + 1, old_btn.y + 1, old_btn.w - 2, old_btn.h - 2, radius > 0 ? radius - 1 : 0, theme_stroke_color_rgb565);
      }
      const auto& current_btn = current_buttons[current_touched_button_index];
      gfx->drawRoundRect(current_btn.x, current_btn.y, current_btn.w, current_btn.h, radius, theme_click_stroke_color_rgb565);
      gfx->drawRoundRect(current_btn.x + 1, current_btn.y + 1, current_btn.w - 2, current_btn.h - 2, radius > 0 ? radius - 1 : 0, theme_click_stroke_color_rgb565);
    }
    last_touched_button_index = current_touched_button_index;
  }

  // --- 2. HOLDING (Hold) ---
  else if (is_touched_now && current_touched_button_index != -1 && current_touched_button_index == last_touched_button_index) {
      unsigned long pressDuration = millis() - touch_start_time;
      if (!long_press_triggered && pressDuration > 800) {
          
          // A. COUNTER RESET
          if (current_buttons[current_touched_button_index].action == "counter") {
              CounterInfo &counter = current_counters[current_touched_button_index];
              counter.currentValue = counter.startValue;
              draw_single_button(current_touched_button_index);
              
              // Notify PC that counter was reset
              Serial.printf("COUNTER_UPDATE:%d:%d:%ld\n", current_page, current_touched_button_index, counter.currentValue);
              long_press_triggered = true;
          }
          
          // B. TIMER RESET
          else if (current_buttons[current_touched_button_index].action == "timer") {
              TimerInfo &timer = current_timers[current_touched_button_index];
              timer.state = TIMER_INACTIVE;
              timer.lastSeconds = timer.duration; 
              draw_single_button(current_touched_button_index);
              
              // Notify PC: Reset
              Serial.printf("TIMER_UPDATE:%d:%d:2:%d\n", current_page, current_touched_button_index, timer.duration);
              long_press_triggered = true;
          }
      }
  }
  
  // --- 3. RELEASE (Release) ---
  else if (!is_touched_now && was_touched) {
    if (last_touched_button_index != -1) {
      const auto& released_btn = current_buttons[last_touched_button_index];
      // A. "goto"
      if (released_btn.action == "goto") {
        int page_index = released_btn.value.toInt() - 1;
        if (page_index >= 0 && page_index < doc["pages"].size()) {
          current_page = page_index;
          draw_page(current_page); 
          last_touched_button_index = -1;
          was_touched = false;
          delay(50);
        }
      }
      
      // B. "timer"
      else if (released_btn.action == "timer") {
        // ONLY IF SHORT PRESS (Long press already reset in Hold section above)
        if (!long_press_triggered) { 
            TimerInfo &timer = current_timers[last_touched_button_index];
            if (timer.state == TIMER_INACTIVE) {
              // --- START / RESUME ---
              timer.state = TIMER_RUNNING;
              
              // Compensate for elapsed time
              unsigned long time_already_passed_ms = 0;
              if (timer.lastSeconds > 0 && timer.lastSeconds < timer.duration) {
                  time_already_passed_ms = (timer.duration - timer.lastSeconds) * 1000;
              }
              timer.startTime = millis() - time_already_passed_ms;

              // Notification (State: 1 = RUNNING)
              int currentDisplaySec = (timer.lastSeconds > 0) ? timer.lastSeconds : timer.duration;
              Serial.printf("TIMER_UPDATE:%d:%d:1:%d\n", current_page, last_touched_button_index, currentDisplaySec);

            } else {
              // --- PAUSE ---
              timer.state = TIMER_INACTIVE;
              // Calculate and save elapsed time
              unsigned long elapsed_sec = (millis() - timer.startTime) / 1000;
              timer.lastSeconds = timer.duration - elapsed_sec;
              
              // Safety: Don't go negative
              if (timer.lastSeconds < 0) timer.lastSeconds = 0;
              draw_single_button(last_touched_button_index);
              
              // Notification (State: 0 = PAUSE)
              Serial.printf("TIMER_UPDATE:%d:%d:0:%d\n", current_page, last_touched_button_index, timer.lastSeconds);
            }
        }
      }

      // C. Other Commands
      else if (released_btn.action == "key" || released_btn.action == "text" || released_btn.action == "app" || released_btn.action == "script" || released_btn.action == "website" || released_btn.action == "media" || released_btn.action == "mouse" || released_btn.action == "sound") {
          send_pc_command(current_page, last_touched_button_index);
      }

      // D. Toggle
      else if (released_btn.action == "toggle") {
          current_toggles[last_touched_button_index].state = !current_toggles[last_touched_button_index].state;
          draw_single_button(last_touched_button_index);
          send_pc_command(current_page, last_touched_button_index);
      }

      // E. Counter (Increment/Decrement)
      else if (released_btn.action == "counter") {
          if (!long_press_triggered) {
              CounterInfo &counter = current_counters[last_touched_button_index];
              if (counter.action == "increment") counter.currentValue++;
              else counter.currentValue--;
              
              draw_single_button(last_touched_button_index);
              
              // Notify PC of new value
              Serial.printf("COUNTER_UPDATE:%d:%d:%ld\n", current_page, last_touched_button_index, counter.currentValue);
          }
      }
      
      // Visual Cleanup
      if (released_btn.action != "goto" && released_btn.action != "toggle" && current_timers[last_touched_button_index].state == TIMER_INACTIVE) {
          gfx->drawRoundRect(released_btn.x, released_btn.y, released_btn.w, released_btn.h, radius, theme_stroke_color_rgb565);
          gfx->drawRoundRect(released_btn.x + 1, released_btn.y + 1, released_btn.w - 2, released_btn.h - 2, radius > 0 ? radius - 1 : 0, theme_stroke_color_rgb565);
      }
      last_touched_button_index = -1;
    }
  }

  was_touched = is_touched_now;
  checkActiveTimers();
#ifdef JC3248W535C
  gfx->flush();
#endif
  delay(20);
}
