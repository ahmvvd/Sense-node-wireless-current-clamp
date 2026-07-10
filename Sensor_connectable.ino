// Initialize libraries
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "esp_sleep.h"

// Ensure extended advertising is enabled for NimBLE
#if !CONFIG_BT_NIMBLE_EXT_ADV
# error Must enable extended advertising, see nimconfig.h file.
#endif

// BLE Service ID
#define CT_SERVICE_UUID        "f7f2a5d4-c360-4857-a249-d5a62dd80e02"
#define CT_CHARACTERISTIC_UUID "8afd9a77-55d5-4bbd-bd21-21393a858648"

// Loop time
static uint32_t advTime = 5 * 1000;
static uint32_t sleepSeconds = 5;

/* Can be one of BLE_HCI_LE_PHY_1M or BLE_HCI_LE_PHY_CODED */
// BLE_HCI_LE_PHY_CODED is only visible to BLE5 devices, and has longer range but smaller data width
static uint8_t primaryPhy = BLE_HCI_LE_PHY_CODED;
static uint8_t secondaryPhy = BLE_HCI_LE_PHY_CODED;

// Callback event
class ServerCallbacks : public NimBLEServerCallbacks {
    // If connected
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        // Print connected device info
        Serial.printf("Client connected:\n%s", connInfo.toString().c_str());
    }

    // If disconnected
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        // Announce and go to sleep
        Serial.printf("Client disconnected - sleeping for %" PRIu32 " seconds\n", sleepSeconds);
        esp_deep_sleep_start();
    }
} serverCallbacks;

// Advertising events
class AdvertisingCallbacks : public NimBLEExtAdvertisingCallbacks {

    // If advertising has stopped
    void onStopped(NimBLEExtAdvertising* pAdv, int reason, uint8_t instId) override {
        Serial.printf("Advertising instance %u stopped\n", instId);
        switch (reason) {
            // Client connecting
            case 0:
                Serial.printf("Client connecting\n");
                return;

            // Advertising timeout
            case BLE_HS_ETIMEOUT:
                Serial.printf("Time expired - sleeping for %" PRIu32 " seconds\n", sleepSeconds);
                break;
            default:
                break;
        }

        esp_deep_sleep_start();
    }
} advertisingCallbacks;

void setup() {
    Serial.begin(115200);

    // Set device name
    NimBLEDevice::init("CT Node");

    // Create server as pServer
    NimBLEServer* pServer = NimBLEDevice::createServer();

    // Register callback event
    pServer->setCallbacks(&serverCallbacks);

    // Create service as pService with UUID listed at the start
    NimBLEService*        pService = pServer->createService(CT_SERVICE_UUID);

    // Create characteristic under pService as pCharacteristic
    NimBLECharacteristic* pCharacteristic =
        pService->createCharacteristic(CT_CHARACTERISTIC_UUID,
                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    // Set characteristic value
    pCharacteristic->setValue("100");

    // Create extended advertisement packet
    NimBLEExtAdvertisement extAdv(primaryPhy, secondaryPhy);

    /** As per Bluetooth specification, extended advertising cannot be both scannable and connectable */
    // Retain connectable to allow sleep and advertisement time change
    extAdv.setConnectable(true);
    extAdv.setScannable(false);

    // Add pService into extended advertisement packet
    extAdv.addServiceUUID(NimBLEUUID(CT_SERVICE_UUID));

    // Packet name
    extAdv.setName("CT Node Extended");

    // Create extended advertising object as pAdvertising
    NimBLEExtAdvertising* pAdvertising = NimBLEDevice::getAdvertising();

    // Register callback events
    pAdvertising->setCallbacks(&advertisingCallbacks);

    /**
     *  NimBLEExtAdvertising::setInstanceData takes the instance ID and
     *  a reference to a `NimBLEExtAdvertisement` object. This sets the data
     *  that will be advertised for this instance ID, returns true if successful.
     *
     *  Note: It is safe to create the advertisement as a local variable if setInstanceData
     *  is called before exiting the code block as the data will be copied.
     */

    // Register advertisement data
    if (pAdvertising->setInstanceData(0, extAdv)) {
        /**
         *  NimBLEExtAdvertising::start takes the advertisement instance ID to start
         *  and a duration in milliseconds or a max number of advertisements to send (or both).
         */
        // Advertise data
        if (pAdvertising->start(0, advTime)) {
            Serial.printf("Started advertising\n");
        } else {
            Serial.printf("Failed to start advertising\n");
        }
    } else {
        Serial.printf("Failed to register advertisement data\n");
    }

    // Set wakeup time
    esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000);
}

void loop() {}
