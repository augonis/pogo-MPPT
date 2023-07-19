#include <Arduino.h>
#include <ESPmDNS.h>
#include "server.h"
#include "sensors.h"
#include "buck.h"
#include "globals.h"
#include <WiFiUdp.h>
#include "AsyncUDP.h"
#include "callibrationServer.h"


TaskHandle_t sendTelemetryTaskHandle;

WiFiUDP Udp;
AsyncUDP aUdp;

static uint16_t UDPListenPort = 5005;

IPAddress telemetryServerIP;
uint16_t telemetryServerPort;

constexpr int meh = sizeof(SensorReadings)+sizeof(BuckSetpoint)+sizeof(EventBits_t);

uint64_t emac;


void sendTelemetryTask(void * parameter){
  emac = ESP.getEfuseMac();
  
  for(;;){
    if (true){
      EventBits_t bits = xEventGroupGetBits(stateEventGroupHandle);
      
      Udp.beginPacket(telemetryServerIP, telemetryServerPort);
      
      Udp.write((uint8_t *)&emac, sizeof(emac));
      Udp.write((uint8_t *)&sensorReadings, sizeof(SensorReadings));
      Udp.write((uint8_t *)&buckSetpoint, sizeof(buckSetpoint));
      Udp.write((uint8_t *)&bits, sizeof(EventBits_t));
      Udp.endPacket();
    }
    
    delay(200);
  }
}

void serverSetup(){
  
  xEventGroupWaitBits(stateEventGroupHandle, STATE_NETWORK_CONNECTED, false, true, portMAX_DELAY);
  
  int n;
  for(;;){
    n = MDNS.queryService("mppt-hub", "udp");
    if (n>0){
      Serial.print(n);
        telemetryServerIP = MDNS.IP(0);
        telemetryServerPort = MDNS.port(0);
      break;
    }
    delay(2000);
  }
  
  if(aUdp.listen(UDPListenPort)) {
        Serial.print("UDP Listening on IP: ");
        Serial.println(WiFi.localIP());
        aUdp.onPacket([](AsyncUDPPacket packet) {
          
        });
    }
  
  xTaskCreate(sendTelemetryTask,
              "sendTelemetryTask",
              3000,
              NULL,
              0,
              &sendTelemetryTaskHandle);
}

void endServer(){
  MDNS.end();
}
