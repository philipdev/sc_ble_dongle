//#include <Arduino.h>

#include <cstdint>
#include <bluefruit.h>
#include "Adafruit_TinyUSB.h"



#define WRITE_MASK 0x0FFFFC3F
#define BIT6 64
#define BIT7 128
#define BIT8 256
#define BIT9 512
#define BUTTON_PIN 13;

#define BUTTONS_REPORT_DESC_GAMEPAD(...)                                     \
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                  \
    HID_USAGE(HID_USAGE_DESKTOP_GAMEPAD),                                    \
    HID_COLLECTION(HID_COLLECTION_APPLICATION),                              \
    HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON), HID_USAGE_MIN(1),                 \
    HID_USAGE_MAX(32), HID_LOGICAL_MIN(0), HID_LOGICAL_MAX(1),               \
    HID_REPORT_COUNT(32), HID_REPORT_SIZE(1),                                \
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_COLLECTION_END

const uint8_t HID_REPORT_DESCRIPTOR[] = {BUTTONS_REPORT_DESC_GAMEPAD()};

BLEClientService serviceButtons("dfd757b2-3779-44c2-b814-150c032ff1a1");
BLEClientCharacteristic characteristicButtons("5a0c806c-eb42-48bd-8429-0e62bf93a4a6");
Adafruit_USBD_HID usbHid(HID_REPORT_DESCRIPTOR, sizeof(HID_REPORT_DESCRIPTOR), HID_ITF_PROTOCOL_NONE, 2, false);



xTaskHandle usbButtonTaskHandle = NULL;
void usbSendreport(uint32_t buttons);

void scanCallback(ble_gap_evt_adv_report_t *report) {
  if (report->type.connectable && Bluefruit.Central.connect(report)) {
    printf("BLE connect\n");
  } else {
    printf("BLE resume scanning\n");
    Bluefruit.Scanner.resume();
  }
}

void connectCallback(uint16_t conn_handle) {
  printf("BLE connected\n");

  if (!serviceButtons.discover(conn_handle) ||
      !Bluefruit.Discovery.discoverCharacteristic(conn_handle, characteristicButtons)) {
    printf("BLE Service not found!\n");
    Bluefruit.disconnect(conn_handle);
    return;
  }

  if (!characteristicButtons.enableNotify()) {
    printf("BLE Failed to enable notifications!\n");
    Bluefruit.disconnect(conn_handle);
  }
}

void notifyCallback(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len) {
  uint32_t buttonState = 0;
  if (len == 4) {
     memcpy(&buttonState, data, 4);
     usbSendreport(buttonState);
  } else {
    printf("Invalid data length in ble notification, expected: 4, actual: %d\n", len);
  }
}

void bleSetup() {
  printf("Initialize start\n");
  Bluefruit.begin(0, 1);
  serviceButtons.begin();
  characteristicButtons.begin();
  characteristicButtons.setNotifyCallback(notifyCallback);
  Bluefruit.setTxPower(4);
  /* Set the device name */
  Bluefruit.setName("nRF52840 USB Gamepad");
  /* Set the LED interval for blinky pattern on BLUE LED */
  Bluefruit.setConnLedInterval(250);
  Bluefruit.Scanner.setRxCallback(scanCallback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Central.setConnectCallback(connectCallback);
  Bluefruit.Discovery.begin();
  Bluefruit.Scanner.start(0);
  printf("Scanning ...\n");
}


// retuns bit2 when rotated to the right, bit1 when rotated to the left, 0
// otherwize.
uint32_t checkRotary(uint32_t buttons, uint32_t lastButtonState, uint32_t bit1, uint32_t bit2) {
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
void usbButtonsTask(void* args) {
    TinyUSBDevice.setManufacturerDescriptor("Untangle Solutions");
    TinyUSBDevice.setProductDescriptor("SC BLE Gamepad");
    //usbHid.getStringDescriptor("SC BLE Gamepad");
    usbHid.begin();
   
   // usbHid.setStringDescriptor("Button plate to USB gamepad");
    while (!TinyUSBDevice.mounted()) {        
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    u_int32_t buttonState = 0; 
    u_int32_t lastButtonState =0;
    u_int32_t reportButtonState =0;
    u_int32_t lastReportButtonState=0;
    TickType_t lastWakeTime = xTaskGetTickCount ();;
    TickType_t minimalDelay = pdMS_TO_TICKS(24);
    while(true) {
      if( xTaskNotifyWait(0x00, UINT32_MAX, &buttonState, portMAX_DELAY ) == pdTRUE ) {  
        
        reportButtonState = checkRotary(buttonState, lastButtonState, BIT6, BIT7) | checkRotary(buttonState, lastButtonState, BIT8, BIT9);
        reportButtonState = reportButtonState | (buttonState & WRITE_MASK);

        if(lastButtonState & (BIT6 | BIT7 | BIT8 | BIT9)) {
          vTaskDelayUntil(&lastWakeTime, minimalDelay); 
        }
        
        for(u_int32_t retry = 0; retry < 3 && !usbHid.sendReport32(0, reportButtonState); retry++) {
          vTaskDelay(pdMS_TO_TICKS(4));
        }
       

        for(u_int32_t b=0; b < 32; b++ ) {
            u_int32_t bit = 1 << b;
           
            if(((lastReportButtonState ^ reportButtonState) & bit) == bit) {
              printf("Button %lu -> %s, report=%08lX, last=%08lX\n", b+1, (bit & reportButtonState) == bit? "UP":"DOWN", reportButtonState, lastReportButtonState);
            }
        }

        //printf("USB Report %08lX, last=%08lX\n", reportButtonState, lastReportButtonState);
        lastButtonState = buttonState;
        lastReportButtonState = reportButtonState;
      } else {
        printf("USB_BUTTON Task xTaskNotifyWait timed out!\n");
      }
    }

    vTaskDelete(NULL);
}


void usbSetup() {
 
  if(xTaskCreate(usbButtonsTask, "USB_BUTTON", 2048, NULL, 2, &usbButtonTaskHandle) == pdFAIL) {
    printf("Failed to create USB_BUTTON Task!\n");
  }
}

void usbSendreport(uint32_t buttons) {
  if(usbButtonTaskHandle != NULL) {
    xTaskNotify(usbButtonTaskHandle, buttons, eSetValueWithOverwrite);
  } else {
    printf("USB_BOTTON Task is not running!\n");
  }
}

void setup() {
  usbSetup();
  bleSetup();
}

void loop() { 
}
