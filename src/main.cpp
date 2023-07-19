#include <Arduino.h>
#include <Preferences.h>

#include "sensors.h"
#include "buck.h"
#include "globals.h"
#include "indicator.h"
#include "backflow.h"
#include "mppt.h"
#include "network.h"
#include "server.h"
#include "cooling.h"
#include "callibrationServer.h"

StaticEventGroup_t errorEventGroup;
EventGroupHandle_t stateEventGroupHandle;

Preferences preferences;

void setup() {
  stateEventGroupHandle = xEventGroupCreateStatic( &errorEventGroup );
  Serial.begin(115200);
  delay(1000);
  
  preferences.begin("mppt", true);
  
  
  if (preferences.getBytes("buckSetpoint", &buckSetpoint, sizeof(buckSetpoint)) < sizeof(buckSetpoint)){
    buckSetpoint = {.minInputVoltage=30, .maxOutputVoltage=28.5, .maxOutputCurrent=16};
  }
  
  if (preferences.getBytes("sensorCal", &sensorCallibration, sizeof(sensorCallibration)) < sizeof(sensorCallibration)){
    sensorCallibration = {.inputVoltageCoefficient=(205.1/5.1), .outputVoltageCoefficient=24.5, .outputCurrentCoefficient=1/0.0015, .fetTemperatureCoefficient=1};
  }
  
  preferences.end();
  
  pinMode(23, INPUT_PULLUP);
  bool callibrationMode = !digitalRead(23);
  
  if(!callibrationMode){// normal running
    ESP_LOGI("system", "Normal running mode");
    startNetwork();
    startIndicator();
    startSensors();
    startBuck();
    startBackflowControl();
    startMppt();
    startCoolingControl();
    
    xEventGroupSetBits(stateEventGroupHandle, STATE_BUCK_ENABLED);
    
    serverSetup();
  }else{// callibration mode
    ESP_LOGI("system", "Callibration mode aaakbgfjahsgftyuiq");
    startNetwork();
    startIndicator();
    startSensors();
    
    startCallibrationServer();
  }
  
  
  
  Serial.println("SetupDone!!!");
}

void loop() {
  // put your main code here, to run repeatedly:
    
  ESP_LOGI("sensors", "V_in=%.4fV V_out=%.4fV I_in=%.4fA I_out=%.4fA P_out=%.4fW T_fet=%.4fC Qout=%.6f Vinmin=%.4fV state=%x samplesPerSecond=%.0f", 
           sensorReadings.inputVoltage, sensorReadings.outputVoltage, sensorReadings.inputCurrent, 
           sensorReadings.outputCurrent, sensorReadings.outputPower, sensorReadings.fetTemperature, 
           sensorReadings.outputCharge, buckSetpoint.minInputVoltage, xEventGroupGetBits(stateEventGroupHandle), sensorReadings.samplesPerSecond);
  
  //ESP_LOGI("errors", "%x", xEventGroupGetBits(stateEventGroupHandle));
  delay(150);
  vTaskDelete(NULL);
}

