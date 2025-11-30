#include "ScreenHelper.h"

void set_brightness(int percent) {
  // Safety check
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  
  current_brightness = percent;
  
  int duty = 0;

  if (percent == 0) {
    duty = 0; // Completely off
  } 
  else if (percent == 100) {
    duty = 255; // Completely on
  } 
  else {
    // HARDWARE LIMIT UPDATE
    // If your screen turns off below 85, your lower limit is too high.
    // 235 out of 255 is approximately 92% power.
    // We're mapping slider's 1% to this 235.
    
    int min_hardware_limit = 30;
    int max_hardware_limit = 255;

    duty = map(percent, 1, 100, min_hardware_limit, max_hardware_limit);
  }
  
  #ifdef GFX_BL
    ledcWrite(BL_PWM_CHANNEL, duty);
  #endif
}

void update_activity() {
  last_activity_time = millis();
  if (is_sleeping) {
    is_sleeping = false;
    // Return to previous brightness
    set_brightness(current_brightness);
    Serial.println("Wake up!");
  }
}

void check_sleep_mode() {
  if (!sleep_enabled || is_sleeping) return;
  if (millis() - last_activity_time > sleep_timeout_ms) {
    is_sleeping = true;
    // Dim the screen (0 turns it completely off, 10 makes it dim)
    // Completely off (0) is usually better.
    #ifdef GFX_BL
      ledcWrite(BL_PWM_CHANNEL, 0); 
    #endif
    Serial.println("Going to sleep...");
  }
}
