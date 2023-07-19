
#include <Arduino.h>
#include <FastLED.h>
#include "pinconfig.h"
#include "globals.h"

CRGB leds[1];

xTaskHandle indicatorTaskHandle;
void indicatorTask(void * parameter);

void startIndicator(){
  FastLED.addLeds<WS2812B, (uint8_t) PinConfig::LED, GRB>(leds, 1);
  FastLED.setBrightness(100);
  leds[0] = CRGB(60, 60, 60);
  FastLED.show();
  
  xTaskCreate(indicatorTask,
              "indicatorTask",
              2000,
              NULL,
              0,
              &indicatorTaskHandle);
  FastLED.show();
  
}

void setIndicator(uint8_t ir, uint8_t ig, uint8_t ib){
  leds[0] = CRGB(ir, ig, ib);
  FastLED.show();
}

void indicatorTask(void * parameter){
  for(;;){
    //EventBits_t uxBits = xEventGroupWaitBits(stateEventGroupHandle, (STATE_BUCK_MODE_SAFED|STATE_BUCK_MODE_CIV|STATE_BUCK_MODE_COV|STATE_SENSORS_READY), false, false, portMAX_DELAY);
    EventBits_t uxBits = xEventGroupGetBits(stateEventGroupHandle);
    
    if (uxBits & STATE_BUCK_MODE_SAFED){
      leds[0] = CRGB(60, 0, 0);
    }else if (uxBits & STATE_BUCK_MODE_CIV){
      leds[0] = CRGB(0, 0, 60);
    }else if (uxBits & STATE_BUCK_MODE_COC){
      leds[0] = CRGB(100, 60, 0);
    }else if (uxBits & STATE_BUCK_MODE_COV){
      leds[0] = CRGB(0, 60, 0);
    }else if (uxBits & STATE_SENSORS_READY){
      //leds[0] = CRGB(60, 0, 60);
    }else {
      
    }
    //ESP_LOGD("indicator", "indicated");
    FastLED.show();
    
    delay(200);
  }
}
