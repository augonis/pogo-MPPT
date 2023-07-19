#include <Arduino.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#include "AsyncUDP.h"
#include <WiFiUdp.h>
#include "callibrationServer.h"
#include "globals.h"
#include "sensors.h"
#include "indicator.h"
#include "pinconfig.h"


AsyncUDP caUdp;

Preferences cPreferences;

TaskHandle_t sendTelemetryCTaskHandle;
TaskHandle_t callibrationTaskHandle;

char *packetData;
int packetDataMaxLength = 0;

struct packetHeader{
  char tag[3];
};

struct floatAnswer{
  char tag[3];
  char key;
  float value;
};


void sendTelemetryCTask(void * parameter){
  for(;;){
    caUdp.printf("ST:Vin=%.4f Vout=%.4f Iout=%.4f", sensorReadings.inputVoltage, sensorReadings.outputVoltage, sensorReadings.outputCurrent);
    //ESP_LOGI("CAL", "T");
    delay(500);
  }
}

float promptFloat(char key, const char* prompt){
  floatAnswer* answer = NULL;
  do{
    if (answer != NULL)caUdp.print("Bad answer, try again");
    caUdp.printf("PR:%c:f:%s", key, prompt);
    ulTaskNotifyTake(true, portMAX_DELAY);
    answer = (floatAnswer*) packetData;
    
    ESP_LOGW("CAL", "got answer %c %f", answer->key, answer->value);
    
  }while(answer->key != key);
  return answer->value;
}


void callibrationTask(void * parameter){
  //enable backflow
  pinMode((uint8_t) PinConfig::Backflow_EN, OUTPUT);
  digitalWrite((uint8_t) PinConfig::Backflow_EN, LOW);
  
  caUdp.print("Ready for callibration");
  //cPreferences.begin("mppt", false);
  
  // callibrate input voltage
  caUdp.print("Apply input voltage");
  float Vin_real = promptFloat('a', "Input voltage=");
  float Vin_sensed = 0;
  caUdp.printf("Got real input voltage Vin_real=%.4f, measuring sensed voltage", Vin_real);
  for(size_t i=0;i<500;i++){
    xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, portMAX_DELAY);
    Vin_sensed += sensorReadings.inputVoltage/500.0f;
  }
  caUdp.printf("Measured input voltage Vin_sensed=%.4f", Vin_sensed);
  sensorCallibration.inputVoltageCoefficient = sensorCallibration.inputVoltageCoefficient*Vin_real/Vin_sensed;
  caUdp.printf("Calculated new input voltage coefficient %.4f", sensorCallibration.inputVoltageCoefficient);
  
  // callibrate output voltage
  caUdp.print("Apply output voltage");
  float Vout_real = promptFloat('b', "Output voltage=");
  float Vout_sensed = 0;
  caUdp.printf("Got real output voltage Vout_real=%.4f, measuring sensed voltage", Vout_real);
  for(size_t i=0;i<500;i++){
    xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, portMAX_DELAY);
    Vout_sensed += sensorReadings.outputVoltage/500.0f;
  }
  caUdp.printf("Measured output voltage Vout_sensed=%.4f", Vout_sensed);
  sensorCallibration.outputVoltageCoefficient = sensorCallibration.outputVoltageCoefficient*Vout_real/Vout_sensed;
  caUdp.printf("Calculated new output voltage coefficient %.4f", sensorCallibration.outputVoltageCoefficient);
  
  // callibrate output current
  caUdp.print("Romove output voltage");
  while (Vout_sensed > 0.05){
    xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, portMAX_DELAY);
    Vout_sensed = sensorReadings.outputVoltage;
  }
  delay(200);
  while (Vout_sensed > 0.05){
    xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, portMAX_DELAY);
    Vout_sensed = sensorReadings.outputVoltage;
  }
  
  setIndicator(255, 0, 0);
  
  pinMode((uint8_t) PinConfig::Bridge_EN, OUTPUT);
  pinMode((uint8_t) PinConfig::Bridge_IN, OUTPUT);
  
  digitalWrite((uint8_t) PinConfig::Bridge_EN, HIGH);
  digitalWrite((uint8_t) PinConfig::Bridge_IN, LOW);
  
  caUdp.print("Apply output current");
  float Iout_real = promptFloat('c', "Output current=");
  float Iout_sensed = 0;
  caUdp.printf("Got real output current Iout_real=%.4f, measuring sensed current", Iout_real);
  for(size_t i=0;i<1500;i++){
    xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, portMAX_DELAY);
    Iout_sensed += sensorReadings.outputCurrent/1500.0f;
  }
  caUdp.printf("Measured output current Iout_sensed=%.4f", Iout_sensed);
  sensorCallibration.outputCurrentCoefficient = sensorCallibration.outputCurrentCoefficient*Iout_real/Iout_sensed;
  caUdp.printf("Calculated new output current coefficient %.7f", sensorCallibration.outputCurrentCoefficient);
  
  // confirm and save
  if (promptFloat('c', "Confirm to save")>0){
    caUdp.print("Saving coeficcients");
    cPreferences.begin("mppt", false);
    if (cPreferences.putBytes("sensorCal", &sensorCallibration, sizeof(sensorCallibration)) < sizeof(sensorCallibration)){
      caUdp.print("Failed to save coeficcients");
    }else{
      caUdp.print("Coeficcients saved");
      cPreferences.end();
    }
  }else{
    caUdp.print("Coeficcient saving canceled");
  }
  
  caUdp.print("Callibration done!");
  for(;;)delay(500);
}


void handlePacket(AsyncUDPPacket& packet){
  
  uint8_t *data = packet.data();
  
  if (!packetData)packetData = (char*)malloc(packet.length());
  else packetData = (char*)realloc(packetData, packet.length());
  
  memcpy(packetData, data, packet.length());
  
  ESP_LOGW("CAL", "got packet %s, len%i", data, packet.length());
  
  if(memcmp(data, "RS:", 3)==0){
    xTaskNotifyGive(callibrationTaskHandle);
  }else{
    ESP_LOGW("CAL", "AAAAARGH %s || %i", ((packetHeader*) data)->tag, sizeof(((packetHeader*) data)->tag));
  }
  
}


void startCallibrationServer(){
  setIndicator(200, 200, 200);
  
  ESP_LOGW("CAL", "startCallibrationServer");
  xEventGroupWaitBits(stateEventGroupHandle, STATE_NETWORK_CONNECTED, false, true, portMAX_DELAY);
  
  
  while (!MDNS.addService("md-mppt-cal", "udp", 5124))delay(1000);
  
  
  
  if(caUdp.listen(5124)) {
        caUdp.onPacket([](AsyncUDPPacket packet) {
          
          caUdp.connect(packet.remoteIP(), packet.remotePort());
          caUdp.onPacket(handlePacket);
          
          xTaskCreate(sendTelemetryCTask,
                      "sendTelemetryCTask",
                      3000,
                      NULL,
                      0,
                      &sendTelemetryCTaskHandle);
          
          xTaskCreate(callibrationTask,
                      "callibrationTask",
                      3000,
                      NULL,
                      0,
                      &callibrationTaskHandle);
        });
  }else{
    ESP_LOGE("system", "failed");
    ESP.restart();
  }
}
