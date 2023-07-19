#include <Arduino.h>
#include "globals.h"
#include "backflow.h"
#include "sensors.h"
#include "pinconfig.h"



xTaskHandle backflowControlTaskHandle;
void backflowControlTask(void * parameter);

void startBackflowControl(){
  
  pinMode((uint8_t) PinConfig::Backflow_EN, OUTPUT);
  digitalWrite((uint8_t) PinConfig::Backflow_EN, HIGH);
  
  xTaskCreate(backflowControlTask,
              "backflowControlTask",
              2000,
              NULL,
              4,
              &backflowControlTaskHandle);
  
}



void backflowControlTask(void * parameter){
  for(;;){
    EventBits_t uxBits = xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, portMAX_DELAY);
    //ESP_LOGI("backflow", "check");
    if (uxBits & STATE_SENSORS_READ_EVENT && sensorReadings.inputVoltage-sensorReadings.outputVoltage > 1.5f){
      digitalWrite((uint8_t) PinConfig::Backflow_EN, LOW);
      xEventGroupSetBits(stateEventGroupHandle, STATE_BACKFLOW_ENABLED);
    }else{
      digitalWrite((uint8_t) PinConfig::Backflow_EN, HIGH);
      xEventGroupClearBits(stateEventGroupHandle, STATE_BACKFLOW_ENABLED);
    }
    delay(500);
  }
}
