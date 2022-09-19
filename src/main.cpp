#include <Arduino.h>

#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>


#define BUTTONS_REPORT_DESC_GAMEPAD(...)                                       \
  HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                      \
      HID_USAGE(HID_USAGE_DESKTOP_GAMEPAD),                                    \
      HID_COLLECTION(HID_COLLECTION_APPLICATION),                              \
      HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON), HID_USAGE_MIN(1),                 \
      HID_USAGE_MAX(32), HID_LOGICAL_MIN(0), HID_LOGICAL_MAX(1),               \
      HID_REPORT_COUNT(32), HID_REPORT_SIZE(1),                                \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_COLLECTION_END

const uint8_t HID_REPORT_DESCRIPTOR[] = {BUTTONS_REPORT_DESC_GAMEPAD()};

typedef void (*debounce_callback_t)();

class debounce {
private:
  int buttonState = HIGH;
  int lastInputState = 0;
  unsigned long consecutiveTime;
  debounce_callback_t press_handler = nullptr;
  debounce_callback_t release_handler = nullptr;

public:
  debounce() {}
  void update(int pinState) {
    if (pinState != lastInputState) {
      lastInputState = pinState;
      consecutiveTime = millis();
    }
    if (consecutiveTime + 50 > millis()) {

      if (buttonState == HIGH && lastInputState == LOW) {
        if (press_handler) {
          press_handler();
        }
      } else if (buttonState == LOW && lastInputState == HIGH) {
        if (release_handler) {
          release_handler();
        }
      }
      buttonState = lastInputState;
    }
  }

  void onpress(debounce_callback_t press_handler) {
    this->press_handler = press_handler;
  }

  void onrelease(debounce_callback_t release_handler) {
    this->release_handler = release_handler;
  }

  bool pressed() { return LOW == buttonState; }
};
#define WRITE_MASK 0x0FFFFC3F
#define BIT6 64
#define BIT7 128
#define BIT8 256
#define BIT9 512
#define BUTTON_PIN 13;

BLEClientService scwheel_service("dfd757b2-3779-44c2-b814-150c032ff1a1");
BLEClientCharacteristic scwheel_buttons("5a0c806c-eb42-48bd-8429-0e62bf93a4a6");
Adafruit_USBD_HID usb_hid(HID_REPORT_DESCRIPTOR, sizeof(HID_REPORT_DESCRIPTOR),
                          HID_ITF_PROTOCOL_NONE, 2, false);
debounce button;
uint32_t lastButtonState = 0;

void button_release_callback() { Serial.println("Button pressed!"); }

void button_press_callback() { Serial.println("Button released!"); }

void scan_callback(ble_gap_evt_adv_report_t *report) {
  if (report->type.connectable && Bluefruit.Central.connect(report)) {
    Serial.println("Connect");
  } else {
    Serial.println("Resume");
    Bluefruit.Scanner.resume();
  }
}

void connect_callback(uint16_t conn_handle) {
  Serial.println("connect_callback");

  if (!scwheel_service.discover(conn_handle) ||
      !Bluefruit.Discovery.discoverCharacteristic(conn_handle, scwheel_buttons)) {
    Serial.println("Service not found!");
    Bluefruit.disconnect(conn_handle);
    return;
  }

  if (!scwheel_buttons.enableNotify()) {
    Serial.println("Failed to enable notifications!");
    Bluefruit.disconnect(conn_handle);
  }
}

// retuns bit2 when rotated to the right, bit1 when rotated to the left, 0
// otherwize.
uint32_t checkRotary(uint32_t buttons, uint32_t bit1, uint32_t bit2) {
  if (lastButtonState == 0 ||
      (buttons & (bit1 | bit2)) == (lastButtonState & (bit1 | bit2))) {
    return 0; // no change
  } else {
    uint32_t currentBit1 = (buttons & bit1) == bit1;
    uint32_t currentBit2 = (buttons & bit2) == bit2;
    uint32_t prevBit1 = (lastButtonState & bit1) == bit1;
    if (currentBit1 == currentBit2) {
      // disengage
      return 0;
    } else if (currentBit2 == prevBit1) {
      return bit2;
    } else {
      return bit1;
    }
  }
}

void sendReport(uint32_t buttonState) {
  uint32_t result = buttonState & WRITE_MASK;
  result |= checkRotary(buttonState, BIT6, BIT7);
  result |= checkRotary(buttonState, BIT8, BIT9);
  usb_hid.sendReport(0, &result, sizeof(result));
  Serial.printf("sendReport: %08X\n", result);
}

void notify_callback(BLEClientCharacteristic *chr, uint8_t *data,
                     uint16_t len) {
  uint32_t buttonState;
  if (len == 4) {
    memcpy(&buttonState, data, 4);
    sendReport(buttonState);
    lastButtonState = buttonState;
  } else {
    Serial.printf("Invalid data length in ble notification, expected: 4, actual: %d\n", len);
  }
}

void setup() {
  usb_hid.begin();
  while (!TinyUSBDevice.mounted())
    delay(1);

  Serial.begin(115200);
  while (!Serial)
    delay(10); // for nrf52840 with native usb
  Serial.println("Initialize start");

  Bluefruit.begin(0, 1);
  scwheel_service.begin();
  scwheel_buttons.begin();
  scwheel_buttons.setNotifyCallback(notify_callback);
  Bluefruit.setTxPower(4);
  /* Set the device name */
  Bluefruit.setName("nRF52840 Dongle");
  /* Set the LED interval for blinky pattern on BLUE LED */
  Bluefruit.setConnLedInterval(250);
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Discovery.begin();
  Bluefruit.Scanner.start(0);
  Serial.println("Scanning ...");
  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  button.onrelease(button_release_callback);
  button.onpress(button_press_callback);
}

void loop() { 
  button.update(digitalRead(PIN_BUTTON1)); 
}
