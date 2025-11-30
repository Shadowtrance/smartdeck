/**
 * Minimal driver for AXS15231B I2C touch controller
 * 
 */

#ifndef AXS15231B_TOUCH_H
#define AXS15231B_TOUCH_H

#include <Arduino.h>
#include <Wire.h>

// Touch hardware configuration
#define AXS15231B_TOUCH_SDA   4
#define AXS15231B_TOUCH_SCL   8
#define AXS15231B_TOUCH_INT   3
#define AXS15231B_TOUCH_ADDR  0x3B

// Touch point structure
struct TouchPoint {
    uint16_t x;
    uint16_t y;
    bool touched;
};

class AXS15231B_Touch {
public:
    AXS15231B_Touch();
    
    // Initialize touch controller
    bool begin();
    
    // Set display rotation and logical dimensions for touch coordinate mapping
    void setRotation(uint8_t rotation, int16_t logicalWidth, int16_t logicalHeight);
    
    // Read touch state
    // Returns true if touch detected, fills point with coordinates
    bool read(TouchPoint &point);
    
    // Check if currently touched (non-blocking)
    bool isTouched();
    
    // Get last touch point
    TouchPoint getLastTouch() { return lastTouch; }
    
private:
    TouchPoint lastTouch;
    uint8_t currentRotation = 0;
    int16_t logicalWidth = 320;
    int16_t logicalHeight = 480;
    
    // AXS15231B touch read command
    static const uint8_t AXS_READ_TOUCHPAD[11];
};

#endif // AXS15231B_TOUCH_H
