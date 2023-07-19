#include <Arduino.h>
#include "pinconfig.h"
#include "globals.h"
#include "sensors.h"


#define FAN_PWM_CH 2
#define FAN_PWM_BITS 8

#define FAN_0_TEMP 60
#define FAN_100_TEMP 90

xTaskHandle fanControlTaskHandle;
void fanControlTask(void * parameter);

uint8_t fanPin = (uint8_t) PinConfig::Fan;

void startCoolingControl(){
  xTaskCreate(fanControlTask,
              "fanControlTask",
              2000,
              NULL,
              0,
              &fanControlTaskHandle);
}

void setFanSpeed(float duty){
  ledcWrite(FAN_PWM_CH, (uint16_t)(((2 << (FAN_PWM_BITS-1)) - 1) * duty*0.01f));
}


void fanControlTask(void * parameter){
  
  pinMode(fanPin, OUTPUT);
  digitalWrite(fanPin, LOW);
  
  auto freq = ledcSetup(FAN_PWM_CH, 16000, FAN_PWM_BITS);
  
  ledcAttachPin(fanPin, FAN_PWM_CH);
  ledcWrite(FAN_PWM_CH, 0);
  float temp;
  
  for(;;){
    temp = sensorReadings.fetTemperature;
    
    setFanSpeed(constrain(map(temp, FAN_0_TEMP, FAN_100_TEMP, 0, 100), 0, 100));
    
    if (temp > 105){
      xEventGroupClearBits(stateEventGroupHandle, STATE_BUCK_ENABLED);
      while(temp > 105){
        delay(1000);
        temp = sensorReadings.fetTemperature;
      }
      xEventGroupSetBits(stateEventGroupHandle, STATE_BUCK_ENABLED);
    }
    
    
    delay(1000);
  }
}
