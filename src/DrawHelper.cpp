#include "DrawHelper.h"

// *** Helper Function: Convert Font Size to GFX Size ***
int mapFontSize(int pixelSize) {
  if (pixelSize <= 14) return 1;
  if (pixelSize <= 22) return 2;
  return 3;
}

// *** Helper Function: Convert Hex Color to RGB565 ***
uint16_t hex_to_rgb565(const char* hex_str) {
  uint32_t hex_val = (uint32_t)strtol(hex_str, NULL, 16);
  uint8_t r = (hex_val >> 16) & 0xFF;
  uint8_t g = (hex_val >> 8) & 0xFF;
  uint8_t b = hex_val & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) |
         (b >> 3);
}

// *** Helper Function: Draw Text on Button ***
void drawButtonText(int x, int y, const char* text) {
  int16_t tx, ty; uint16_t tw, th;
  gfx->setTextSize(1);
  gfx->getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
  int text_x = x + (CELL_W - tw) / 2;
  int text_y = y + (CELL_H - th) / 2;
  gfx->fillRect(x + 5, y + 5, CELL_W - 10, CELL_H - 10, theme_btn_color_rgb565);
  gfx->setTextColor(theme_text_color_rgb565);
  gfx->setCursor(text_x, text_y);
  gfx->print(text);
}

// *** TJpgDec Output Callback Function ***
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  int16_t jpg_rel_x = jpegInfo.x_offset + x;
  int16_t jpg_rel_y = jpegInfo.y_offset + y;

  int r = jpegInfo.radius;
  int btn_w = jpegInfo.maxWidth;
  int btn_h = jpegInfo.maxHeight;
  long r_squared = (long)r * r;

  for (int16_t py = 0; py < h; py++) {
    for (int16_t px = 0; px < w; px++) {

      int16_t btn_rel_x = jpg_rel_x + px;
      int16_t btn_rel_y = jpg_rel_y + py;

      if (btn_rel_x < 0 || btn_rel_x >= btn_w || btn_rel_y < 0 || btn_rel_y >= btn_h) {
        continue;
      }

      bool draw_pixel = true;
      if (btn_rel_x < r && btn_rel_y < r) {
        if (((long)r - btn_rel_x) * ((long)r - btn_rel_x) + ((long)r - btn_rel_y) * ((long)r - btn_rel_y) > r_squared) {
          draw_pixel = false;
        }
      } else if (btn_rel_x >= (btn_w - r) && btn_rel_y < r) {
        if (((long)btn_rel_x - (btn_w - 1 - r)) * ((long)btn_rel_x - (btn_w - 1 - r)) + ((long)r - btn_rel_y) * ((long)r - btn_rel_y) > r_squared) {
          draw_pixel = false;
        }
      } else if (btn_rel_x < r && btn_rel_y >= (btn_h - r)) {
        if (((long)r - btn_rel_x) * ((long)r - btn_rel_x) + ((long)btn_rel_y - (btn_h - 1 - r)) * ((long)btn_rel_y - (btn_h - 1 - r)) > r_squared) {
          draw_pixel = false;
        }
      } else if (btn_rel_x >= (btn_w - r) && btn_rel_y >= (btn_h - r)) {
        if (((long)btn_rel_x - (btn_w - 1 - r)) * ((long)btn_rel_x - (btn_w - 1 - r)) + ((long)btn_rel_y - (btn_h - 1 - r)) * ((long)btn_rel_y - (btn_h - 1 - r)) > r_squared) {
          draw_pixel = false;
        }
      }

      if (draw_pixel) {
        int16_t draw_x = jpegInfo.x + btn_rel_x;
        int16_t draw_y = jpegInfo.y + btn_rel_y;

        uint16_t color = bitmap[py * w + px];
        gfx->drawPixel(draw_x, draw_y, color);
      }
    }
  }

  return true;
}

void checkActiveTimers() {
  unsigned long now = millis();
  
  for (int i = 0; i < current_timers.size(); i++) {
    TimerInfo &timer = current_timers[i];
    // If INACTIVE, skip to next button
    if (timer.state == TIMER_INACTIVE) continue;

    ButtonInfo &button = current_buttons[i];
    JsonObject button_cfg = doc["pages"][current_page]["buttons"][i];

    // 1. IS TIMER RUNNING?
    if (timer.state == TIMER_RUNNING) {
      unsigned long elapsed_ms = now - timer.startTime;
      long remaining_sec = timer.duration - (elapsed_ms / 1000);

      // A. Has time expired?
      if (remaining_sec < 0) {
        timer.state = TIMER_FINISHED;
        timer.startTime = now; // Save finish time (for flash effect)
        timer.lastSeconds = -1;
        
        // Notify PC: Time's Up!
        Serial.printf("TIMER_DONE:%d:%d\n", current_page, i);
        
        // Visual Effects: Red BG
        gfx->fillRoundRect(button.x, button.y, button.w, button.h, CORNER_RADIUS, 0xF800);
        gfx->drawRoundRect(button.x, button.y, button.w, button.h, CORNER_RADIUS, 0xF800);
        
        // Draw "00:00" text
        // ... (Metin çizim kodları aynı) ...
        int label_size_px = button_cfg["labelSize"] | 18;
        int final_font_size = mapFontSize(label_size_px);
        if (final_font_size < 3) final_font_size = 3;
        
        gfx->setTextSize(final_font_size);
        uint16_t text_color_to_use = theme_text_color_rgb565;
        const char* label_color_hex = button_cfg["labelColor"];
        if (label_color_hex) text_color_to_use = hex_to_rgb565(label_color_hex);
        gfx->setTextColor(text_color_to_use);
        
        int16_t tx, ty; uint16_t tw, th;
        gfx->getTextBounds("00:00", 0, 0, &tx, &ty, &tw, &th);
        gfx->setCursor(button.x + (button.w - tw) / 2, button.y + (button.h - th) / 2);
        gfx->print("00:00");
        
      }
      // B. Time not expired, did second change?
      else if (remaining_sec != timer.lastSeconds) {
        timer.lastSeconds = remaining_sec;
        char time_str[6];
        sprintf(time_str, "%02ld:%02ld", remaining_sec / 60, remaining_sec % 60);
        
        // Redraw button (Background, border, new time)
        // ... (Drawing code remains the same) ...
        uint16_t btn_color_rgb565 = theme_btn_color_rgb565;
        const char* btn_color_hex = button_cfg["btnColor"] | "DEFAULT";
        if (strcmp(btn_color_hex, "DEFAULT") != 0) btn_color_rgb565 = hex_to_rgb565(btn_color_hex);
        
        gfx->fillRoundRect(button.x, button.y, button.w, button.h, CORNER_RADIUS, btn_color_rgb565);
        gfx->drawRoundRect(button.x, button.y, button.w, button.h, CORNER_RADIUS, theme_stroke_color_rgb565);
        gfx->drawRoundRect(button.x + 1, button.y + 1, button.w - 2, button.h - 2, CORNER_RADIUS > 0 ? CORNER_RADIUS - 1 : 0, theme_stroke_color_rgb565);
        
        int label_size_px = button_cfg["labelSize"] | 18;
        int final_font_size = mapFontSize(label_size_px);
        if (final_font_size < 3) final_font_size = 1;
        
        gfx->setTextSize(final_font_size);
        uint16_t text_color_to_use = theme_text_color_rgb565;
        const char* label_color_hex = button_cfg["labelColor"];
        if (label_color_hex) text_color_to_use = hex_to_rgb565(label_color_hex);
        gfx->setTextColor(text_color_to_use);
        
        int16_t tx, ty; uint16_t tw, th;
        gfx->getTextBounds(time_str, 0, 0, &tx, &ty, &tw, &th);
        gfx->setCursor(button.x + (button.w - tw) / 2, button.y + (button.h - th) / 2);
        gfx->print(time_str);
      }
    }
    
    // 2. TIMER FINISHED (TIMER_FINISHED) - Flashing and Auto Reset
    else if (timer.state == TIMER_FINISHED) {
      
      unsigned long flashDuration = now - timer.startTime;
      
      // AUTO RESET MOMENT (e.g: after 10 seconds)
      if (flashDuration > 10000) {
          timer.state = TIMER_INACTIVE;
          timer.startTime = 0;
          timer.lastSeconds = timer.duration; // Reset to beginning
          
          draw_single_button(i); // Restore screen to previous state
          
          // --- NEWLY ADDED SECTION: Notify PC of Reset ---
          // State 2 = RESET
          Serial.printf("TIMER_UPDATE:%d:%d:2:%d\n", current_page, i, timer.duration);
          // -------------------------------------------------
          
          continue;
      }
      
      // Flashing Effect (Remains the same)
      const uint16_t flash_colors[5] = {0xF800, 0xFFE0, 0x07E0, 0x001F, 0xF81F};
      int color_index = (flashDuration / 400) % 5;
      uint16_t flash_color = flash_colors[color_index];
      
      gfx->fillRoundRect(button.x, button.y, button.w, button.h, CORNER_RADIUS, flash_color);
      gfx->drawRoundRect(button.x, button.y, button.w, button.h, CORNER_RADIUS, 0xFFFF);
      gfx->drawRoundRect(button.x + 1, button.y + 1, button.w - 2, button.h - 2, CORNER_RADIUS > 0 ? CORNER_RADIUS - 1 : 0, 0xFFFF);
      
      int label_size_px = button_cfg["labelSize"] | 18;
      int final_font_size = mapFontSize(label_size_px);
      if (final_font_size < 3) final_font_size = 3;
      
      gfx->setTextSize(final_font_size);
      uint16_t text_color_to_use = theme_text_color_rgb565;
      const char* label_color_hex = button_cfg["labelColor"];
      if (label_color_hex) text_color_to_use = hex_to_rgb565(label_color_hex);
      gfx->setTextColor(text_color_to_use);
      
      int16_t tx, ty; uint16_t tw, th;
      gfx->getTextBounds("00:00", 0, 0, &tx, &ty, &tw, &th);
      gfx->setCursor(button.x + (button.w - tw) / 2, button.y + (button.h - th) / 2);
      gfx->print("00:00");
    }
  }
}

void draw_single_button(int btn_index) {
  int COLS = doc["grid"]["cols"] | 3;
  int ROWS = doc["grid"]["rows"] | 3;

  // --- POSITION CALCULATIONS (App.js Compatible) ---
#if defined(SMALL)
  int gridAvailableHeight = screenHeight - 60;
  int start_y_offset = 25;
#elif defined(MEDIUM) || defined(LARGE)
  int gridAvailableHeight = screenHeight - 90;
  int start_y_offset = 50;
#endif

  int totalCellHeight = ROWS * CELL_H;
  int remainingSpaceY = gridAvailableHeight - totalCellHeight;
  
  int gapY = 0;
  int padY_top = 0;
  int numGapsY = ROWS - 1;

  if (remainingSpaceY < 0) {
      gapY = -2;
      padY_top = 0;
  } else if (numGapsY > 0) {
      padY_top = 2;
      int space_for_gaps = remainingSpaceY - 4;
      gapY = space_for_gaps / numGapsY;
  } else {
      gapY = 0;
      padY_top = remainingSpaceY / 2;
  }
  padY_top += start_y_offset;

  int gapX = 0;
  if (COLS > 1) {
      gapX = (screenWidth - (COLS * CELL_W)) / (COLS + 1);
  } else {
      gapX = (screenWidth - CELL_W) / 2;
  }

  int shadow_offset_x = 5;
  int totalPaddingSpaceX = screenWidth - (COLS * CELL_W) - ((COLS - 1) * gapX);
  int padX_left = (totalPaddingSpaceX - shadow_offset_x) / 2;
  if (padX_left < 0) padX_left = 0;

  int r = btn_index / COLS;
  int c = btn_index % COLS;

  int x_pos = padX_left + (c * CELL_W) + (c * gapX);
  int y_pos = padY_top + (r * CELL_H) + (r * gapY);
  
  // ---------------------------------------------------------

  JsonArray pages_array = doc["pages"];
  JsonArray page_data = pages_array[current_page]["buttons"];
  ButtonInfo& btn_info = current_buttons[btn_index];
  JsonObject button_cfg = page_data[btn_index];
  int radius = CORNER_RADIUS;

  // --- 0. SHADOW DRAWING [NEWLY ADDED] ---
  // Before the button itself, we draw a dark colored box 5px to the right and 5px down.
  // This creates a shadow effect under the button.
  gfx->fillRoundRect(x_pos + 5, y_pos + 5, CELL_W, CELL_H, radius, theme_shadow_color_rgb565);

  // 1. BACKGROUND COLOR (Fallback)
  uint16_t bg_color = theme_btn_color_rgb565;
  const char* custom_color = button_cfg["btnColor"];
  if (custom_color && strlen(custom_color) > 0 && strcmp(custom_color, "DEFAULT") != 0) {
      bg_color = hex_to_rgb565(custom_color);
  }
  
  if (btn_info.action == "toggle" && current_toggles[btn_index].state) {
      bg_color = current_toggles[btn_index].onColor;
  }

  // Draw background
  gfx->fillRoundRect(x_pos, y_pos, CELL_W, CELL_H, radius, bg_color);

  // 2. ICON / JPEG DRAWING
  String fileToDraw = "";
  if (btn_info.action != "timer") {
      if (btn_info.action == "toggle") {
          bool isStateOn = current_toggles[btn_index].state;
          const char* iconOffName = button_cfg["toggleData"]["iconOff"];
          const char* iconOnName = button_cfg["toggleData"]["iconOn"];

          if (isStateOn) {
              if (iconOnName) fileToDraw = "/" + String(iconOnName);
          } else {
              if (iconOffName) fileToDraw = "/" + String(iconOffName);
          }
      } 
      else {
          const char* icon_file_ptr = button_cfg["icon"];
          if (icon_file_ptr) {
              fileToDraw = "/" + String(icon_file_ptr);
          }
      }

      if (fileToDraw.length() > 1) { 
          if (SD.exists(fileToDraw)) {
              uint16_t jpg_w = 0, jpg_h = 0;
              TJpgDec.getSdJpgSize(&jpg_w, &jpg_h, fileToDraw.c_str());
              
              if (jpg_w > 0 && jpg_h > 0) {
                  jpegInfo.x = x_pos;
                  jpegInfo.y = y_pos;
                  jpegInfo.maxWidth = CELL_W; 
                  jpegInfo.maxHeight = CELL_H;
                  jpegInfo.x_offset = (CELL_W - jpg_w) / 2;
                  jpegInfo.y_offset = (CELL_H - jpg_h) / 2;
                  jpegInfo.radius = radius;
                  TJpgDec.drawSdJpg(0, 0, fileToDraw.c_str());
              } else {
                  Serial.printf("ERR: JPG size 0 for %s\n", fileToDraw.c_str());
              }
          } else {
              Serial.printf("ERR: File not found -> %s\n", fileToDraw.c_str());
              gfx->setTextColor(0xFFFF);
              gfx->setCursor(x_pos + 5, y_pos + 5);
              gfx->print("!"); 
          }
      }
  }

  // 3. Border
  gfx->drawRoundRect(x_pos, y_pos, CELL_W, CELL_H, radius, theme_stroke_color_rgb565);
  gfx->drawRoundRect(x_pos + 1, y_pos + 1, CELL_W - 2, CELL_H - 2, radius > 0 ? radius - 1 : 0, theme_stroke_color_rgb565);

  // 4. Text Drawing
  const char* label_text = button_cfg["label"];
  int label_size_px = button_cfg["labelSize"] | 18;
  char time_str[10];
  char counter_str[32]; 

  if (btn_info.action == "timer") {
      int displayVal = current_timers[btn_index].duration;
      if (current_timers[btn_index].state == TIMER_INACTIVE && current_timers[btn_index].lastSeconds != -1) {
          displayVal = current_timers[btn_index].lastSeconds;
      }
      sprintf(time_str, "%02d:%02d", displayVal / 60, displayVal % 60);
      label_text = time_str;
  }
  else if (btn_info.action == "counter") {
      long val = current_counters[btn_index].currentValue;
      sprintf(counter_str, "%ld", val);
      label_text = counter_str; 
  }
  else {
      label_text = NULL; 
  }

  if (label_text) {
      int final_font_size = mapFontSize(label_size_px);
      if ((btn_info.action == "timer" || btn_info.action == "counter") && final_font_size < 3) {
          final_font_size = 1;
      }
      
      gfx->setTextSize(final_font_size);
      uint16_t text_color_to_use = theme_text_color_rgb565;
      const char* label_color_hex = button_cfg["labelColor"];
      if (label_color_hex) text_color_to_use = hex_to_rgb565(label_color_hex);
      
      gfx->setTextColor(text_color_to_use);

      int16_t tx, ty; uint16_t tw, th;
      gfx->getTextBounds(label_text, 0, 0, &tx, &ty, &tw, &th);
      int text_x = x_pos + (CELL_W - tw) / 2;
      int text_y = y_pos + (CELL_H - th) / 2;
      gfx->setCursor(text_x, text_y);
      gfx->print(label_text);
  }
}

void draw_page(int page_index) {
  Serial.printf("Drawing page index %d...\n", page_index);
  current_buttons.clear();
  current_timers.clear();
  
  // Clear screen
  gfx->fillScreen(theme_bg_color_rgb565);
  
  int COLS = doc["grid"]["cols"] | 3;
  int ROWS = doc["grid"]["rows"] | 3;
  
  current_buttons.resize(ROWS * COLS);
  current_timers.resize(ROWS * COLS);
  current_counters.clear();
  current_counters.resize(ROWS * COLS);
  current_toggles.clear();
  current_toggles.resize(ROWS * COLS);

  JsonArray pages_array = doc["pages"];
  if (page_index >= pages_array.size()) {
    Serial.printf("Error: Page index %d is out of bounds!\n", page_index);
    return;
  }

  // --- 1. FRAME DRAWING ---
  // A rounded rectangle surrounding the content like in the original code
  int frame_margin = 2; // 10px space from edges
  int frame_x = frame_margin;
  int frame_y = frame_margin;
  int frame_w = screenWidth - (2 * frame_margin);
  int frame_h = screenHeight - (2 * frame_margin);
  
  gfx->drawRoundRect(frame_x, frame_y, frame_w, frame_h, CORNER_RADIUS, theme_stroke_color_rgb565);
  // Optional: Draw another one inside to make the frame more prominent (Thickness effect)
  gfx->drawRoundRect(frame_x + 1, frame_y + 1, frame_w - 2, frame_h - 2, CORNER_RADIUS, theme_stroke_color_rgb565);


  // --- 2. POSITION CALCULATIONS (App.js Compatible) ---
  
#if defined(SMALL)
  int gridAvailableHeight = screenHeight - 60;
  int start_y_offset = 25; // Header space
#elif defined(MEDIUM) || defined(LARGE)
  int gridAvailableHeight = screenHeight - 90;
  int start_y_offset = 50; // Header space
#endif

  int totalCellHeight = ROWS * CELL_H;
  int remainingSpaceY = gridAvailableHeight - totalCellHeight;
  int gapY = 0;
  int padY_top = 0;
  int numGapsY = ROWS - 1;

  if (remainingSpaceY < 0) { gapY = -2; padY_top = 0; }
  else if (numGapsY > 0) {
      padY_top = 2;
      int space_for_gaps = remainingSpaceY - 4;
      gapY = space_for_gaps / numGapsY;
  } else { gapY = 0; padY_top = remainingSpaceY / 2; }
  padY_top += start_y_offset;

  int gapX = 0;
  if (COLS > 1) gapX = (screenWidth - (COLS * CELL_W)) / (COLS + 1);
  else gapX = (screenWidth - CELL_W) / 2;

  int shadow_offset_x = 5;
  int totalPaddingSpaceX = screenWidth - (COLS * CELL_W) - ((COLS - 1) * gapX);
  int padX_left = (totalPaddingSpaceX - shadow_offset_x) / 2;
  if (padX_left < 0) padX_left = 0;

  // ---------------------------------------------------------

  // Device Title (Center Header)
  const char* title_text = doc["title"] | "Stream Deck";
  gfx->setTextSize(1);
  gfx->setTextColor(theme_text_color_rgb565);
  int16_t tx, ty; uint16_t tw, th;
  gfx->getTextBounds(title_text, 0, 0, &tx, &ty, &tw, &th);
  
  // Center title vertically (within 50px header area)
  int header_center_y = (50 / 2)-15;
  gfx->setCursor((screenWidth - tw) / 2, header_center_y + (th/2));
  gfx->print(title_text);

  // NOTE: The line between (drawFastHLine) was removed from here.

  // Calculate and Draw Buttons
  JsonArray page_data = pages_array[page_index]["buttons"];
  int btn_index = 0;
  
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      int current_button_vector_index = r * COLS + c;
      
      // USE CALCULATED POSITIONS
      int x_pos = padX_left + (c * CELL_W) + (c * gapX);
      int y_pos = padY_top + (r * CELL_H) + (r * gapY);

      ButtonInfo btn_info;
      btn_info.x = x_pos; btn_info.y = y_pos; btn_info.w = CELL_W; btn_info.h = CELL_H;

      if (btn_index >= page_data.size() || page_data[btn_index].isNull()) {
        btn_info.defined = false;
      } else {
        JsonObject button_cfg = page_data[btn_index];
        btn_info.defined = true;
        btn_info.action = button_cfg["type"] | "none";

        // Assign Action Values
        if (btn_info.action == "goto") { btn_info.value = button_cfg["page"] | 1; }
        else if (btn_info.action == "key") { btn_info.value = button_cfg["combo"] | ""; }
        else if (btn_info.action == "text") { btn_info.value = button_cfg["text"] | ""; }
        else if (btn_info.action == "app") { btn_info.value = ""; }
        else if (btn_info.action == "script") { btn_info.value = ""; } 
        else if (btn_info.action == "website") { btn_info.value = ""; }
        else if (btn_info.action == "media") { btn_info.value = ""; } 
        else if (btn_info.action == "mouse") { btn_info.value = ""; } 
        else if (btn_info.action == "http") { btn_info.value = button_cfg["http"]["url"] | ""; }
        else if (btn_info.action == "sound") { btn_info.value = ""; }
        
        // Timer
        else if (btn_info.action == "timer") {
            int duration = button_cfg["duration"] | 0;
            btn_info.value = String(duration);
            current_timers[current_button_vector_index].duration = duration;
            current_timers[current_button_vector_index].state = TIMER_INACTIVE;
            current_timers[current_button_vector_index].lastSeconds = duration;
        }
        // Counter
        else if (btn_info.action == "counter") {
            int start_val = button_cfg["counterStartValue"] | 0;
            const char* action_type = button_cfg["counterAction"] | "increment";
            current_counters[current_button_vector_index].startValue = start_val;
            current_counters[current_button_vector_index].currentValue = start_val;
            current_counters[current_button_vector_index].action = String(action_type);
            btn_info.value = String(start_val);
        }
        // Toggle
        else if (btn_info.action == "toggle") {
            btn_info.value = "toggle";
            current_toggles[current_button_vector_index].state = true;
            const char* on_color_hex = button_cfg["toggleData"]["onColor"];
            if (on_color_hex) {
                current_toggles[current_button_vector_index].onColor = hex_to_rgb565(on_color_hex);
            } else {
                current_toggles[current_button_vector_index].onColor = 0x07E0;
            }
        }
        else { btn_info.value = ""; }

        // Save to vector
        current_buttons[current_button_vector_index] = btn_info;
        
        // DO THE DRAWING
        draw_single_button(current_button_vector_index);
      }
      btn_index++;
    }
  }

  // Page Name (Bottom Bar - Footer)
  const char* page_name_text = pages_array[page_index]["name"] | "Page";
  int footer_y = screenHeight - 30;
  
  gfx->setTextSize(1);
  gfx->setTextColor(theme_text_color_rgb565);
  gfx->getTextBounds(page_name_text, 0, 0, &tx, &ty, &tw, &th);
  gfx->setCursor((screenWidth - tw) / 2, footer_y);
  gfx->print(page_name_text);

  gfx->setTextSize(1);
  Serial.printf("Page %d drawn successfully.\n", page_index + 1);
}
