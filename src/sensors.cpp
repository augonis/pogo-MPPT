#include <SPI.h> // not sure why this is needed
#include <Adafruit_ADS1X15.h>
#include "pinconfig.h"
#include "sensors.h"
#include "globals.h"
#include "statmath.h"

SensorReadings sensorReadings;
SensorCallibration sensorCallibration;


#define NTC_RESISTANCE 10e3f
#define NTC_B 3900
#define NTC_T0 298.15
#define NTC_LOW_SIDE_RESISTANCE 11e3f

//#define NTC_R_INF NTC_RESISTANCE*exp(-NTC_B/NTC_T0)
#define NTC_R_INF 0.020851618



RunningMedian3<float> TfetMedian3{};
EWMA<float> TfetEwma{80};

RunningMedian3<float> IoutMedian3{};
EWMA<float> IoutEwma{80};

EWMA<float> VoutEwma{80};

Adafruit_ADS1015 ads;

xTaskHandle adsTaskHandle;
xTaskHandle NTCTaskHandle;
void adsTask(void * parameter);
void NTCTask(void * parameter);

unsigned long nowMicros;
unsigned long lastSensorReadMicros = micros();

void ICACHE_RAM_ATTR AdsAlertISR_() {
  vTaskNotifyGiveFromISR(adsTaskHandle, NULL);
}
void startSensors(){
  
  
  if (!Wire.begin((uint8_t) PinConfig::I2C_SDA, (uint8_t) PinConfig::I2C_SCL, 400000UL)) {
    ESP_LOGE("main", "Failed to initialize Wire");
    return;
  }
  
  if (!ads.begin(0x48)) {
    ESP_LOGE("main", "Failed to initialize ADS");
    return;
  }
  ads.setDataRate(RATE_ADS1115_475SPS);
  attachInterrupt(digitalPinToInterrupt((uint8_t) PinConfig::ADC_ALERT), AdsAlertISR_, FALLING); // TODO rising
  
  xTaskCreatePinnedToCore(adsTask,
              "adsTask",
              3000,
              NULL,
              6,
              &adsTaskHandle,
              0);
  
  xTaskCreate(NTCTask,
              "NTCTask",
              3000,
              NULL,
              4,
              &NTCTaskHandle);
  delay(10);
  
}


void adsTask(void * parameter){
  float v;
  xEventGroupSetBits(stateEventGroupHandle, STATE_SENSORS_READY);
  lastSensorReadMicros = micros();
  for(;;){
    // measure input voltage
    ads.setGain(GAIN_ONE);
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_3, false);
    ulTaskNotifyTake(true, portMAX_DELAY);
    v = ads.computeVolts(ads.getLastConversionResults());
    sensorReadings.inputVoltage = v * sensorCallibration.inputVoltageCoefficient;
    
    // measure output voltage
    //ads.setGain(GAIN_ONE);
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_2, false);
    ulTaskNotifyTake(true, portMAX_DELAY);
    v = ads.computeVolts(ads.getLastConversionResults());
    sensorReadings.outputVoltage = v * sensorCallibration.outputVoltageCoefficient;
    VoutEwma.add(sensorReadings.outputVoltage);
    sensorReadings.outputVoltage = VoutEwma.get();
    
    // measure output current
    ads.setGain(GAIN_SIXTEEN);
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, false);
    ulTaskNotifyTake(true, portMAX_DELAY);
    v = ads.computeVolts(ads.getLastConversionResults());
    
    //sensorReadings.outputCurrent = -v / OUTPUT_CURRENT_SHUNT_VALUE;
    IoutEwma.add(IoutMedian3.next(-v * sensorCallibration.outputCurrentCoefficient));
    sensorReadings.outputCurrent = IoutEwma.get();
    
    VoutEwma.add(sensorReadings.outputVoltage);
    sensorReadings.outputPower = sensorReadings.outputCurrent*VoutEwma.get();
    
    sensorReadings.inputCurrent = sensorReadings.outputCurrent*sensorReadings.outputVoltage/sensorReadings.inputVoltage;
    
    //ESP_LOGD("adsTask", "readings done");
    nowMicros = micros();
    sensorReadings.samplesPerSecond = 1e6f/(nowMicros-lastSensorReadMicros);
    sensorReadings.outputCharge += (nowMicros-lastSensorReadMicros)/1e6/60/60*sensorReadings.outputCurrent;
    
    lastSensorReadMicros = nowMicros;
    //Serial.println(sensorReadings.outputCurrent);
    
    xEventGroupSetBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT);
    //delay(50);
  }
}

float adc2Celsius(float adc){
  // invalid adc 4095
  if (adc >= 4080)
      return NAN;
  
  float Vout=3.3*(adc/4095.0); // calc for ntc
  float Rout= NTC_LOW_SIDE_RESISTANCE*3.3/Vout-NTC_LOW_SIDE_RESISTANCE;//(NTC_LOW_SIDE_RESISTANCE*Vout/(3.3-Vout));

  float temp=(NTC_B/log(Rout/NTC_R_INF))-273.15; // calc for temperature
  
  if (temp < -200 or temp > 300) {
      return NAN;
  }
  return temp;
}

void NTCTask(void * parameter){
  float temp;
  for(;;){
    temp = adc2Celsius(analogRead((uint8_t) PinConfig::NTC));
    if(!isnan(temp)) TfetEwma.add(TfetMedian3.next(temp));
    
    sensorReadings.fetTemperature = TfetEwma.get();
    delay(50);
  }
}