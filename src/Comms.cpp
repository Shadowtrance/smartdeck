#include "Comms.h"

void send_pc_command(int page, int index) {
  char command[64];
  
  // Check if button is Toggle
  if (current_buttons[index].action == "toggle") {
      // If Toggle: BTN:Page:Index:State (e.g: BTN:0:14:1)
      int stateVal = current_toggles[index].state ? 1 : 0;
      sprintf(command, "BTN:%d:%d:%d\n", page, index, stateVal);
  } else {
      // If normal button, use old format: BTN:Page:Index
      sprintf(command, "BTN:%d:%d\n", page, index);
  }

#if DONGLE_MODE == 1
  // Send via ESP-NOW
  esp_now_send(receiver_mac, (uint8_t*)command, strlen(command) + 1);
  Serial.printf("ESP-NOW Sent: %s", command); 
#else 
  // Send via USB Serial
  Serial.printf("%s", command);
#endif
}
