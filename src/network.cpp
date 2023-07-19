#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "server.h"
#include "globals.h"
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

xTaskHandle networkTaskHandle;
void networkTask(void * parameter);

void onOTAStart(){
  SPIFFS.end();
  endServer();
}

char hostname[21];



void startNetwork(){
  xTaskCreate(networkTask,
              "networkTask",
              4000,
              NULL,
              0,
              &networkTaskHandle);
}


void networkTask(void * parameter){
  
  snprintf(hostname, 21, "MD-MPPT-%llX", ESP.getEfuseMac());
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    ESP_LOGW("network", "WiFi Failed!");
    delay(5000);
  }
  
  ESP_LOGI("network", "WiFi connected");
  ESP_LOGI("network", "IP address: %s", WiFi.localIP().toString());
  ESP_LOGI("network", "Hostname: %s", hostname);
  
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();
  ArduinoOTA.onStart(onOTAStart);
  
  xEventGroupSetBits(stateEventGroupHandle, STATE_NETWORK_CONNECTED);
  
  for(;;){
    ArduinoOTA.handle();
    delay(100);
  }
  
}