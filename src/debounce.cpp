#include "debounce.h"

void debounce::update(int pinState) {
    if (pinState != lastInputState) {
      lastInputState = pinState;
      consecutiveTime = millis();
    }
    if (consecutiveTime + 50 > millis()) {

      if (buttonState == LOW && lastInputState == HIGH) {
        if (press_handler) {
          press_handler();
        }
      } else if (buttonState == HIGH && lastInputState == LOW) {
        if (release_handler) {
          release_handler();
        }
      }
      buttonState = lastInputState;
    }
  }

  void debounce::onpress(debounce_callback_t press_handler) {
    this->press_handler = press_handler;
  }

  void debounce::onrelease(debounce_callback_t release_handler) {
    this->release_handler = release_handler;
  }

  bool debounce::pressed() { 
    return LOW == this->buttonState; 
  }
