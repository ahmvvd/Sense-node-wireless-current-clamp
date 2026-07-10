// Wholy authored by Alston
// Initialize libraries
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "esp_sleep.h"

// Power mode dependencies
#define WAKEUP_GPIO    GPIO_NUM_5     // Only RTC IO are allowed
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  5          // Sleep duration

RTC_DATA_ATTR bool power = false;
// end of power mode dependencies

// Ensure extended advertising is enabled for NimBLE
#if !CONFIG_BT_NIMBLE_EXT_ADV
# error Must enable extended advertising, see nimconfig.h file.
#endif

// BLE Service ID
#define CT_SERVICE_UUID        "f7f2a5d4-c360-4857-a249-d5a62dd80e02"
#define CT_CHARACTERISTIC_UUID "8afd9a77-55d5-4bbd-bd21-21393a858648"

// Current sensor variables
#define ADC_INPUT 2
#define HOME_VOLTAGE 240.0
#define CALIBRATION 71.0
#define DC_OFFSET 2328.0;

double dcOffset = 0;

// Loop time
static uint32_t advTime = 500;
static uint32_t sleepSeconds = 5;

/* Can be one of BLE_HCI_LE_PHY_1M or BLE_HCI_LE_PHY_CODED */
// BLE_HCI_LE_PHY_CODED is only visible to BLE5 devices, and has longer range but smaller data width
static uint8_t primaryPhy = BLE_HCI_LE_PHY_CODED;
static uint8_t secondaryPhy = BLE_HCI_LE_PHY_CODED;

void powerDown(int reason){
    if (reason == 0){
        Serial.println("Power down");
    }

    esp_deep_sleep_start();
}

void powerState(){
    //Print the wakeup reason for ESP32
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_GPIO:    Serial.println("Wakeup caused by GPIO"); 

      // Power state handler
      // If powered off,
      if(!power){
        // Turn back on
        Serial.print("Power up, ");
        power = true;
      }else {
        // Turn off
        Serial.println("Power down");
        power = false;
        powerDown(0);
      }
    break;

    case ESP_SLEEP_WAKEUP_TIMER:    
      // Keep waking up
      Serial.println("Wakeup caused by timer");
    break;

    case ESP_SLEEP_WAKEUP_ULP:     Serial.println("Wakeup caused by ULP"); 
    break;

    case ESP_SLEEP_WAKEUP_UNDEFINED: 
        Serial.println("On reset");
        power = true;
    break;

    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); 
    break;
  }

  Serial.println("back to sleep for " + String(TIME_TO_SLEEP) + " Seconds"); 
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Active high power button
  esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKEUP_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH);

  // Internal pulldown
  gpio_pullup_dis(WAKEUP_GPIO);
  gpio_pulldown_en(WAKEUP_GPIO);
}

// Advertising handler
class AdvertisingCallbacks : public NimBLEExtAdvertisingCallbacks {

    // If advertising has stopped
    void onStopped(NimBLEExtAdvertising* pAdv, int reason, uint8_t instId) override {
        Serial.printf("Advertising instance %u stopped\n", instId);
        switch (reason) {
            // Advertising timeout
            case BLE_HS_ETIMEOUT:
                Serial.printf("Time expired - sleeping for %" PRIu32 " seconds\n", sleepSeconds);
                break;
            default:
                break;
        }
        
        power = true;
        powerDown(1);
    }
} advertisingCallbacks;

void setup() {
    Serial.begin(9600);
    delay(1000);

    powerState();

    // Set device name
    NimBLEDevice::init("CT Node");

    // Create extended advertisement packet
    NimBLEExtAdvertisement extAdv(primaryPhy, secondaryPhy);

    /** As per Bluetooth specification, extended advertising cannot be both scannable and connectable */
    // Non-scannable and non-connectable for maximum power saving
    extAdv.setConnectable(false);
    extAdv.setScannable(false);

    // Packet name
    extAdv.setName("CT Node (shouting)");

    // Create extended advertising object as pAdvertising
    NimBLEExtAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

    // Register callback events
    pAdvertising->setCallbacks(&advertisingCallbacks);

    // Register service
    extAdv.addServiceUUID(NimBLEUUID(CT_SERVICE_UUID));

    // String sensorValue = String(analogRead(A0));

    analogSetAttenuation(ADC_11db);
    analogReadResolution(12);

    double sumSquares = 0;
    int samples = 6000;

    for (int i = 0; i < samples; i++) {
        double raw = analogRead(ADC_INPUT) - DC_OFFSET;
        sumSquares += raw * raw;
    }

    double rms = sqrt(sumSquares / samples);
    double amps = rms * (CALIBRATION / (4096.0 / 3.3));
    if (amps < 0.4) amps = 0;
    
    Serial.print("Amps: ");
    Serial.print(amps);

    // Register data to be sent
    extAdv.setServiceData(
        NimBLEUUID(CT_SERVICE_UUID),
        String(amps).c_str()
    );

    // Register advertisement data
    if (pAdvertising->setInstanceData(0, extAdv)) {
        // Advertise data
        if (pAdvertising->start(0, advTime)) {
            Serial.printf("Started advertising\n");
        } else {
            Serial.printf("Failed to start advertising\n");
        }
    } else {
        Serial.printf("Failed to register advertisement data\n");
    }

    pinMode(WAKEUP_GPIO, INPUT_PULLDOWN);
}

void loop() {
    if(digitalRead(WAKEUP_GPIO)){
        powerDown(0);
    }
}
