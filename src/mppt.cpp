#include <Arduino.h>
#include "globals.h"
#include "sensors.h"
#include "buck.h"
#include "mppt.h"
#include <cstdlib>

xTaskHandle mpptTaskHandle;
void mpptTask(void * parameter);

uint64_t lastTime;
uint64_t now;

float lastPower;
float lastCharge;
float power;
float delta = 0.05;

void startMppt(){
  xTaskCreate(mpptTask,
              "mpptTask",
              1000,
              NULL,
              4,
              &mpptTaskHandle);
  
}



void mpptTask(void * parameter){
  
  xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, portMAX_DELAY);
  lastPower = sensorReadings.outputPower;
  
  lastTime = micros();
  
  for(;;){
    delay(500);
    EventBits_t uxBits = xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, portMAX_DELAY);
    if (uxBits & STATE_BUCK_MODE_CIV){
      now = micros();
      
      power = (sensorReadings.outputCharge-lastCharge)/(now-lastTime);
      
      lastCharge = sensorReadings.outputCharge;
      lastTime = now;
      
      if (power < lastPower) delta *= -1;
      lastPower = power;
      
      //buckSetpoint.minInputVoltage =  constrain(buckSetpoint.minInputVoltage + delta, sensorReadings.outputVoltage + 2, 35.0f);
      buckSetpoint.minInputVoltage =  constrain(sensorReadings.inputVoltage + delta, sensorReadings.outputVoltage + 2, 31.0f);
      //std::fmax(sensorReadings.outputVoltage + 2, buckSetpoint.minInputVoltage + delta);
    }
  }
}
