// Bluetooth dependencies
#include <Arduino.h>
#include <NimBLEDevice.h>

#if !CONFIG_BT_NIMBLE_EXT_ADV
# error Must enable extended advertising, see nimconfig.h file.
#endif

#define SERVICE_UUID        "f7f2a5d4-c360-4857-a249-d5a62dd80e02"
#define CHARACTERISTIC_UUID "8afd9a77-55d5-4bbd-bd21-21393a858648"

static const NimBLEAdvertisedDevice* advDevice;
static uint32_t                      scanInterval = 97;
static uint32_t                      scanWindow = 67;
static uint32_t                      scanTime  = 0 * 1000; // In milliseconds, 0 = scan forever
static uint32_t                      sleepSeconds = 5;

// Select PHYs to connect with
/* BLE_GAP_LE_PHY_CODED_MASK | BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK */
static uint8_t connectPhys = BLE_GAP_LE_PHY_CODED_MASK ;
// end of Bluetooth

// OLED dependencies
#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);
// end of OLED dependencies

// SD dependencies
#include <SPI.h>
#include "FS.h"
#include "SD.h"

RTC_DATA_ATTR unsigned int minute = -1;
// end of SD dependencies

// Append file
void appendFile(fs::FS &fs, const char *path, const char *message){

    File file = fs.open(path, FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open");
        u8g2.clearBuffer(); 
        u8g2.drawStr(5,15,"Failed to open"); 
        u8g2.sendBuffer(); 
        return;
    }
    if (file.print(message)) {
        Serial.printf("Write success to %s\n", path);
    } else {
        Serial.println("Failed to append");
        u8g2.clearBuffer(); 
        u8g2.drawStr(5,15,"Failed to append"); 
        u8g2.sendBuffer(); 
    }
    file.close();
}

// Create file
void writeFile(fs::FS &fs, const char *path, const char *message) {

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create");
    u8g2.clearBuffer(); 
    u8g2.drawStr(5,15,"Failed to create"); 
    u8g2.sendBuffer(); 
  }
  if (file.print(message)) {
    Serial.printf("File created at %s\n", path);
  } else {
    Serial.println("Failed to write");
    u8g2.clearBuffer(); 
    u8g2.drawStr(5,15,"Failed to write"); 
    u8g2.sendBuffer(); 
  }
  file.close();
}

// Advertisement received
class scanCallbacks : public NimBLEScanCallbacks {
    // When received
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {

        // Print out all received advertisement
        // Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());

        // If advertisement match our service
        if (advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
            // Stop scanning
            NimBLEDevice::getScan()->stop();

            // Notify
            Serial.printf("Found Our Service\n");
            u8g2.clearBuffer();

            // Print received data
            Serial.println(
                advertisedDevice->getServiceData().c_str()
            );
            
            // Write data to SD card
            String data = String(++minute) + ",1," + advertisedDevice->getServiceData().c_str() + "," + String(millis()) + "\n";
            appendFile(SD, "/data.csv", data.c_str());

            // Display time found, session time and measurement
            u8g2.drawStr(5,30,String(minute).c_str());
            u8g2.drawStr(5,45,advertisedDevice->getServiceData().c_str());
            u8g2.drawStr(5,15,(String("Found on ") + millis() + " ms").c_str());
            u8g2.sendBuffer(); 

            esp_deep_sleep_start();
        }
    }

    // Process scan result
    void onScanEnd(const NimBLEScanResults& results, int rc) override { 
        u8g2.drawStr(5,30,"Missed"); 
        u8g2.sendBuffer(); 
        Serial.printf("Missed\n");  
        esp_deep_sleep_start(); 
    }
} scanCallbacks;

void setup() {
    Serial.begin(9600);

    // Initialize OLED
    u8g2.begin();
    // Choose font
    u8g2.setFont(u8g2_font_ncenB08_tr);
    // Clear display
    u8g2.clearBuffer();

    // Set wakeup time
    esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000);

    // Initialize SD card
    if (!SD.begin()) {
        // If no SD card is mounted
        Serial.println("Card Mount Failed");
        u8g2.drawStr(5,15,"Card Mount Failed");
        u8g2.sendBuffer(); 
        esp_deep_sleep_start();
    } else{
        // SD card found

        // Create file if no data file exist
        const char *path = "/data.csv";
        File file = SD.open(path);
        if (!file) {
            Serial.println("Creating new file");
            writeFile(SD, path, "minute,node,measurement,response time\n");
        }
        file.close();

        // Set device name
        NimBLEDevice::init("CT Logger");

        // Create scanning instance as pScan
        NimBLEScan* pScan = NimBLEDevice::getScan();

        // Register scanning callbacks
        pScan->setScanCallbacks(&scanCallbacks);

        // Set scanning interval and scanning window
        pScan->setInterval(scanInterval);
        pScan->setWindow(scanWindow);

        // Active scanning uses more power
        pScan->setActiveScan(false);

        // Start scanning
        pScan->start(scanTime);
        u8g2.drawStr(5,15,"Scanning");
        Serial.printf("Scanning\n");
        u8g2.sendBuffer(); 
    }
}

void loop() {}
