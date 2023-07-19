#pragma once

struct BuckSetpoint{
  float minInputVoltage;
  float maxOutputVoltage;
  float maxOutputCurrent;
};

extern BuckSetpoint buckSetpoint;

void startBuck();

void setBuckMinInputVoltage(float voltage);
void setBuckMaxOutputVoltage(float voltage);
void setBuckmaxOutputCurrent(float current);


#define BuckSetpointFormat "fff"
#define BuckSetpointFieldNames "\"minInputVoltage\", \"maxOutputVoltage\", \"maxOutputCurrent\""