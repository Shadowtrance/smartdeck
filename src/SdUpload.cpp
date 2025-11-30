#include "SdUpload.h"

void cleanUnusedIcons() {
  Serial.println("Starting smart icon cleanup...");
  std::set<String> requiredIcons;
  
  JsonArray pages_array = doc["pages"];
  
  // STEP 1: Extract list of required files
  for (JsonVariant page : pages_array) {
    JsonArray buttons_array = page["buttons"];
    for (JsonVariant btn : buttons_array) {
      if (btn.isNull()) continue;

      // A. Add Main Icon (For normal buttons and Toggle OFF/Fallback)
      const char* icon_file = btn["icon"];
      if (icon_file) {
        requiredIcons.insert(String(icon_file));
      }

      // B. Add Toggle Button Icons (CRITICAL FIX)
      // Now we also protect paths in toggleData in JSON
      if (!btn["toggleData"].isNull()) {
          const char* iconOn = btn["toggleData"]["iconOn"];
          if (iconOn) requiredIcons.insert(String(iconOn));

          const char* iconOff = btn["toggleData"]["iconOff"];
          if (iconOff) requiredIcons.insert(String(iconOff));
      }
    }
  }

  Serial.printf("Found %d unique required icons in config file.\n", requiredIcons.size());

  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open root dir for cleanup.");
    return;
  }

  int deletedCount = 0;

  // STEP 2: Scan SD Card and delete files not in the list
  while (File file = root.openNextFile()) {
    String fileName = String(file.name());
    String filePath = "/" + fileName;

    // Only check image files
    if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg") || fileName.endsWith(".JPG") || fileName.endsWith(".JPEG")) {

      // If file is NOT in the list, delete it
      if (requiredIcons.find(fileName) == requiredIcons.end()) {
        Serial.printf(" - Deleting unused icon: %s\n", filePath.c_str());
        if (SD.remove(filePath.c_str())) {
          deletedCount++;
        } else {
          Serial.printf("   ! Failed to delete %s\n", filePath.c_str());
        }
      }
    }
    file.close();
  }

  root.close();
  Serial.printf("Cleanup complete. Deleted %d unused icons.\n", deletedCount);
}

void handleUsbUpload() {
  Serial.println("READY");

  File usbUploadFile;
  bool in_upload = true;
  unsigned long uploadStartTime = millis();

  // (Previous USB upload logic remains the same)
  while (in_upload) {

    // 60 second general timeout
    if (millis() - uploadStartTime > 60000) {
      Serial.println("ERR_TIMEOUT");
      in_upload = false;
      break;
    }

    if (Serial.available() > 0) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      uploadStartTime = millis(); // Reset timeout on each command

      if (cmd.startsWith("FILE:")) {
        int firstColon = cmd.indexOf(':');
        int secondColon = cmd.indexOf(':', firstColon + 1);

        if (firstColon == -1 || secondColon == -1) {
          Serial.println("ERR_CMD_FORMAT");
          continue;
        }

        String filename = "/" + cmd.substring(firstColon + 1, secondColon);
        long fileSize = cmd.substring(secondColon + 1).toInt();

        if (fileSize == 0) {
          Serial.println("ERR_FILE_SIZE");
          continue;
        }

        if (SD.exists(filename)) {
          SD.remove(filename);
        }

        usbUploadFile = SD.open(filename, FILE_WRITE);
        if (!usbUploadFile) {
          Serial.println("ERR_FILE_CREATE");
        } else {
          Serial.println("OK_FILE");

          long remaining = fileSize;
          unsigned long fileStartTime = millis();

          while (remaining > 0) {
            if (Serial.available() > 0) {
              byte b = Serial.read();
              usbUploadFile.write(b);
              remaining--;
              fileStartTime = millis();
            } else {
              if (millis() - fileStartTime > 5000) {
                Serial.println("ERR_DATA_TIMEOUT");
                break;
              }
              delay(1);
            }
          }
          
          usbUploadFile.close();
          if (remaining == 0) {
            Serial.println("OK_DATA");
          } else {
            Serial.println("ERR_DATA_INCOMPLETE");
          }
        }
      } else if (cmd == "END_UPLOAD") {
        in_upload = false;
      } else {
        Serial.println("ERR_UNKNOWN_CMD");
      }
    }
    delay(1);
  }

  Serial.println("DONE_REBOOT");
  delay(100);

  File file = SD.open("/esp_config.json", FILE_READ);
  bool config_ok = false;
  if (file) {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (!error) {
      config_ok = true;
    }
  }

  if (config_ok) {
    Serial.println("Config loaded, running smart cleanup...");
    cleanUnusedIcons();
  } else {
    Serial.println("Failed to load new config, skipping cleanup.");
  }

  Serial.println("Rebooting in 3 seconds...");
  delay(3000);
  ESP.restart();
}
