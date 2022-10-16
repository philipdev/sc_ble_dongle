#ifndef _DEBOUNCE_H_

#define _DEBOUNCE_H_
#include <Arduino.h>

typedef void (*debounce_callback_t)();

class debounce {
private:
  int buttonState = HIGH;
  int lastInputState = HIGH;
  unsigned long consecutiveTime;
  debounce_callback_t press_handler = nullptr;
  debounce_callback_t release_handler = nullptr;

public:
  debounce() {}
  void update(int pinState);
  void onpress(debounce_callback_t press_handler);
  void onrelease(debounce_callback_t release_handler);
  bool pressed();
};


#endif