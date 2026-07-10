#include <Arduino.h>

#define WAKEUP_GPIO    GPIO_NUM_5     // Only RTC IO are allowed
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  5          // Sleep duration

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool power = false;

void setup() {
  Serial.begin(115200);
  delay(3000);

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
        Serial.println("back to sleep for " + String(TIME_TO_SLEEP) + " Seconds"); 
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
        power = true;
      }else {
        // Turn off
        Serial.println("Power down");
        power = false;
      }
    break;

    case ESP_SLEEP_WAKEUP_TIMER:   Serial.println("Wakeup caused by timer"); 
      // Keep waking up
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      Serial.println("Back to sleep for " + String(TIME_TO_SLEEP) + " Seconds"); 
    break;

    case ESP_SLEEP_WAKEUP_ULP:     Serial.println("Wakeup caused by ULP"); 
    break;

    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); 
    break;
  }

  // Active high power button
  esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKEUP_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH);

  // Internal pulldown
  gpio_pullup_dis(WAKEUP_GPIO);
  gpio_pulldown_en(WAKEUP_GPIO);

  //Go to sleep now
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}

void loop() {
  //This is not going to be called
}
