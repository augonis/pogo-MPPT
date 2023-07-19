#pragma once


void startSensors();


struct SensorReadings{
  float inputVoltage;
  float outputVoltage;
  float inputCurrent;
  float outputCurrent;
  float fetTemperature;
  float outputPower;
  float samplesPerSecond;
  float outputCharge;
};

struct SensorCallibration{
  float inputVoltageCoefficient;
  float outputVoltageCoefficient;
  float outputCurrentCoefficient;
  float fetTemperatureCoefficient;
};

extern SensorReadings sensorReadings;
extern SensorCallibration sensorCallibration;

#define SensorReadingsFormat "ffffffIf"
#define SensorReadingsFieldNames "\"inputVoltage\", \"outputVoltage\", \"inputCurrent\", \"outputCurrent\", \"fetTemperature\", \"outputPower\", \"samplesPerSecond\", \"outputCharge\""

