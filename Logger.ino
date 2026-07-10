// Wholy authored by Alston
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
// end of Bluetooth dependencies

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

// Power state depencies
#define WAKEUP_GPIO    GPIO_NUM_4     // Only RTC IO are allowed
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  5            // Sleep duration

RTC_DATA_ATTR bool power = false;
// end of power state dependencies

void powerDown(int reason){
    // Stop scanning
    NimBLEDevice::getScan()->stop();

    // If triggered by power button,
    if (reason == 0){
        Serial.println("Power down");
        u8g2.clearBuffer();
        u8g2.drawStr(5,15,String("Power down").c_str());
        u8g2.sendBuffer();
        delay(1000);
        u8g2.clearBuffer();

        // Turn display off
        u8g2.setPowerSave(1);

        // Active high power button
        esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKEUP_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH);
    
        // Internal pulldown
        gpio_pullup_dis(WAKEUP_GPIO);
        gpio_pulldown_en(WAKEUP_GPIO);
    }

    esp_deep_sleep_start();
}

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

// Power state handler
void powerState(){
    //Print the wakeup reason for ESP32
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
        // Power button pressed
        case ESP_SLEEP_WAKEUP_EXT1:    Serial.println("Wakeup caused by EXT1"); 
            // If previously powered off,
            if(!power){
                // Turn back on
                Serial.println("Power up");
                power = true;
            }else {
                // Turn off
                power = false;
                powerDown(0);
            }
        break;

        // Usual wakeup
        case ESP_SLEEP_WAKEUP_TIMER:   Serial.println("Wakeup caused by timer"); 
            // Keep waking up
            Serial.println("Return from sleep"); 
        break;

        case ESP_SLEEP_WAKEUP_ULP:     Serial.println("Wakeup caused by ULP"); break;

        case ESP_SLEEP_WAKEUP_UNDEFINED: 
            Serial.println("On reset");
            power = true;
        break;

        default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }

    // Keep waking up
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Back to sleep for " + String(TIME_TO_SLEEP) + " Seconds"); 

    // Active high power button
    esp_sleep_enable_ext1_wakeup_io(1ULL << WAKEUP_GPIO, ESP_EXT1_WAKEUP_ANY_HIGH);

    // Internal pulldown
    gpio_pullup_dis(WAKEUP_GPIO);
    gpio_pulldown_en(WAKEUP_GPIO);
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

            powerDown(1);
        }
    }

    // Process scan result
    void onScanEnd(const NimBLEScanResults& results, int rc) override { 
        u8g2.drawStr(5,30,"Missed"); 
        u8g2.sendBuffer(); 
        Serial.printf("Missed\n");

        powerDown(1);
    }
} scanCallbacks;

void setup() {
    Serial.begin(9600);

    // Lower CPU clock
    setCpuFrequencyMhz(80);

    // Initialize OLED
    u8g2.begin();
    // Choose font
    u8g2.setFont(u8g2_font_ncenB08_tr);
    // Clear display
    u8g2.clearBuffer();

    powerState();

    // Initialize SD card
    if (!SD.begin()) {
        // If no SD card is mounted
        Serial.println("Card Mount Failed");
        u8g2.drawStr(5,15,"Card Mount Failed");
        u8g2.sendBuffer(); 
        powerDown(1);
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
        pinMode(WAKEUP_GPIO, INPUT_PULLDOWN);
    }
}

void loop() {
    if(digitalRead(WAKEUP_GPIO)){
        // Stop scanning
        NimBLEDevice::getScan()->stop();
        powerDown(0);
    }
}
