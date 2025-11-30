#pragma once
#include "SmartDeck.h"

int mapFontSize(int pixelSize);
uint16_t hex_to_rgb565(const char* hex_str);
void drawButtonText(int x, int y, const char* text);

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

void draw_single_button(int btn_index);
void draw_page(int page_index);
void checkActiveTimers();
