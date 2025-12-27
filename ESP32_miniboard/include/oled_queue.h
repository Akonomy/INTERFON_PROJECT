#ifndef OLED_QUEUE_H
#define OLED_QUEUE_H

#include <Arduino.h>

// reuse your existing API
#include "oled.h"

#define OLED_QUEUE_MAX_ITEMS 5
#define OLED_QUEUE_EXPIRE_WAITING_MS 3000UL

enum OLED_MessageType : uint8_t {
  OLED_MSG_TEXT = 0,
  OLED_MSG_NUMBER,
  OLED_MSG_PASSWORD
};

struct OLED_Message {
  bool valid = false;

  uint8_t id = 0;
  uint8_t priority = 255;      // 0 = highest
  uint8_t minSeconds = 2;      // min time on screen

  uint8_t extraOnOverwrite = 0; // 0–3 seconds allowed


  uint32_t activatedAtMs = 0;  // when it started being displayed
  uint32_t enqueuedAtMs = 0;   // when added to queue

  OLED_MessageType type;

  // payload
  String text;
  uint8_t number = 0;
  String password;


};

// ============ Public API ============

// call periodically
void OLED_Refresh();

// request display items (all params optional via defaults)
void OLED_RequestDisplayText(
  const String &text = "",
  uint8_t id = 0,
  uint8_t priority = 255,
  uint8_t minSeconds = 2,
    uint8_t extraOnOverwrite=0
);

void OLED_RequestDisplayNumber(
  uint8_t number = 0,
  uint8_t id = 0,
  uint8_t priority = 255,
  uint8_t minSeconds = 2,
   uint8_t extraOnOverwrite=0
);

void OLED_RequestDisplayPassword(
  const String &password = "",
  uint8_t id = 0,
  uint8_t priority = 255,
  uint8_t minSeconds = 2,
   uint8_t extraOnOverwrite=0
);

// should be called once in setup
void OLED_QueueInit();

#endif
