

#include <Arduino.h>
#include "driver/ledc.h"
#include "pinconfig.h"
#include "globals.h"
#include "buck.h"
#include "sensors.h"

BuckSetpoint buckSetpoint = {.minInputVoltage=30, .maxOutputVoltage=3.6*4, .maxOutputCurrent=16};


#define PWM_FREQUENCY 39000


xTaskHandle buckTaskHandle;
void buckTask(void * parameter);

void setBuckMinInputVoltage(float voltage){buckSetpoint.minInputVoltage = voltage;}
void setBuckMaxOutputVoltage(float voltage){buckSetpoint.maxOutputVoltage = voltage;}
void setBuckmaxOutputCurrent(float current){buckSetpoint.maxOutputCurrent = current;}


static constexpr uint8_t pwmCh_IN = 0;
static constexpr uint8_t pwmCh_EN = 1;
const uint8_t pwmResolution = 11;
const ledc_mode_t ledcMode = LEDC_LOW_SPEED_MODE;
static constexpr float MinDutyCycleLS = 0.06f; // to keep the HS bootstrap circuit running

uint16_t pwmHS = 0;
uint16_t pwmLS = 0;

uint16_t pwmMaxLS = 0;

float outInVoltageRatio = 0;


const uint16_t pwmMax = ((2 << (pwmResolution - 1)) - 1);
const uint16_t pwmMaxHS = (pwmMax * (1.0f - MinDutyCycleLS));
const uint16_t pwmMinLS = (std::ceil((float) pwmMax * MinDutyCycleLS)); // keeping the bootstrap circuit powered
const uint16_t pwmMinHS = (pwmMinLS * 2 / 3);

const TickType_t sensorTimeoutTicks = 100 / portTICK_PERIOD_MS;


void init_pwm(int channel, int pin, uint32_t freq) {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
            .speed_mode       = ledcMode,
            .duty_resolution  = (ledc_timer_bit_t) pwmResolution,
            .timer_num        = LEDC_TIMER_0,
            .freq_hz          = freq,
            .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
            .gpio_num       = pin,
            .speed_mode     = ledcMode,
            .channel        = (ledc_channel_t) channel,
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_0,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void update_pwm(int channel, uint32_t duty) {
    ESP_ERROR_CHECK(ledc_set_duty(ledcMode, (ledc_channel_t) channel, duty));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(ledcMode, (ledc_channel_t) channel));
}

uint16_t computePwmMaxLs(uint16_t pwmHS, uint16_t pwmMax, float voltageRatio) {
  // prevents boosting and "low side burning" due to excessive LS current
  // (non-diode behavior, negative reverse inductor current)

  const float margin = 0.99f; // TODO what does this model or represent, remove?

  // this is very sensitive to voltageRatio errors! at VR=0.64 a -5% error causes a 13% deviation of pwmMaxLs !
  constexpr float voltageMaxErr = 0.02f; // inc -> safer, less efficient

  // WCEF = worst case error factor
  // compute the worst case error (Vout too low, Vin too high)
  constexpr float voltageRatioWCEF = (1.f - voltageMaxErr) / (1.f + voltageMaxErr); // < 1.0

  voltageRatio = std::max<float>(voltageRatio, 0.01f); // the greater, the safer

  const float pwmMaxLsWCEF = (1 / (voltageRatio*voltageRatioWCEF) - 1) / (1 / voltageRatio - 1); // > 1


  auto pwmMaxLs = (1 / voltageRatio - 1) * (float) pwmHS / pwmMaxLsWCEF; // the lower, the safer

  // pwmMaxLs = std::min<float>(pwmMaxLs, (float)pwmHS); // TODO explain why this is necessary
  // I guess it can be a little more
  // At which duty cycle (HS) does coil current stop touching zero? see if-block below vvv
  // ^^ https://github.com/fl4p/fugu-mppt-firmware/issues/1

  // the extra 5% below fixes reverse current at 39khz, Vin55, Vout27, dc1000
  // TODO pwm=1390 40.8/26.6

  // TODO remove the 1.05?
  if (pwmMaxLs < (pwmMax - pwmHS)  ) { // * 1.05f
      // DCM (Discontinuous Conduction Mode)
      // this is when the coil current is still touching zero
      // it'll stop for higher HS duty cycles
      pwmMaxLs = std::min<float>(pwmMaxLs, (float) pwmHS * 1.0f); // TODO explain why this is necessary
  } else {
      pwmMaxLs = (float)(pwmMax - pwmHS);
  }

  //ESP_LOGI("dbg", "pwmMaxLs=%f, (float) (pwmMax - pwmHS)=%f, pwmMax=%hu, pwmHS=%hu", pwmMaxLs, (float) (pwmMax - pwmHS), pwmMax, pwmHS);
  
  // todo beyond pwmMaxLs < (pwmMax - pwmHS), can we reduce the worst-case error assumption to boost eff?
  // or just replace  with pwmMaxLs < (pwmMax - pwmHS) * pwmMaxLsWCEF and remove scaling pwmMaxLs by pwmMaxLsWCEF

  return (uint16_t) (pwmMaxLs * margin);
}

void lowSideMinDuty() {
  if (pwmLS > pwmMinLS + 10) {
      ESP_LOGW("pwm", "set low-side PWM to minimum %hu -> %hu (vRatio=%.3f, pwmMaxLS=%hu)", pwmLS, pwmMinLS,
                outInVoltageRatio, pwmMaxLS);
  }
  pwmLS = pwmMinLS;
  update_pwm(pwmCh_EN, pwmHS + pwmLS);
}

void pwmPerturb(int16_t direction) {
  pwmHS = constrain(pwmHS + direction, pwmMinHS, pwmMaxHS);
  
  pwmMaxLS = computePwmMaxLs(pwmHS, pwmMax, outInVoltageRatio);
  
  // "fade-in" the low-side duty cycle
  pwmLS = constrain(pwmLS + 3, pwmMinLS, pwmMaxLS);
  
  
  ESP_LOGD("buck", "Buck switching at pwmHS=%i (%i, %i) pwmLS=%i (%i, %i)", pwmHS, pwmMinHS, pwmMaxHS, pwmLS, pwmMinLS, pwmMaxLS);
  
  update_pwm(pwmCh_IN, pwmHS);
  update_pwm(pwmCh_EN, pwmHS + pwmLS);
}

void updateLowSideMaxDuty(float outInVoltageRatio_) {
  // voltageRatio = Vout/Vin

  outInVoltageRatio = outInVoltageRatio_;

  pwmMaxLS = computePwmMaxLs(pwmHS, pwmMax, outInVoltageRatio);
  pwmMaxLS = std::max(pwmMinLS, pwmMaxLS);

  if (pwmLS > pwmMaxLS) {
      pwmLS = pwmMaxLS;
      update_pwm(pwmCh_EN, pwmHS + pwmLS); // instantly commit if limit decreases
  }
}

void disable() {
  if (pwmHS > pwmMinHS)
      ESP_LOGW("pwm", "PWM disabled");
  pwmHS = 0;
  pwmLS = 0;
  update_pwm(pwmCh_IN, 0);
  update_pwm(pwmCh_EN, 0);
}


void startBuck(){
  
  init_pwm(pwmCh_IN, (uint8_t) PinConfig::Bridge_IN, PWM_FREQUENCY);
  init_pwm(pwmCh_EN, (uint8_t) PinConfig::Bridge_EN, PWM_FREQUENCY);
  
  xTaskCreatePinnedToCore(buckTask,
              "buckTask",
              3000,
              NULL,
              5,
              &buckTaskHandle, 1);
}

bool buckControllLoop(){
  float Vin = sensorReadings.inputVoltage;
  float Vout = sensorReadings.outputVoltage;
  float Iout = sensorReadings.outputCurrent;
  
    //xEventGroupClearBits(stateEventGroupHandle, (STATE_BUCK_MODE_CIV | STATE_BUCK_MODE_COC | STATE_BUCK_MODE_COV | STATE_BUCK_MODE_DISABLED));
    //xEventGroupSetBits(stateEventGroupHandle, (STATE_BUCK_MODE_CIV | STATE_BUCK_MODE_COC | STATE_BUCK_MODE_COV | STATE_BUCK_MODE_DISABLED));
  
  updateLowSideMaxDuty(Vout/Vin);
  
  if (Iout < 0.2f){
    // try to prevent voltage boost and disable low side for low currents
    lowSideMinDuty();
  }
  
  // mode checks
  int16_t dPWM = 1;
  if (Vin < buckSetpoint.minInputVoltage){ // CIV mode
    if(unlikely(Vin-Vout < 0.5f)){
      ESP_LOGD("buck", "triggered input undervoltage Vin=%.4f < 1.5", Vin);
      return false;
    }
    
    dPWM = (Vin-buckSetpoint.minInputVoltage)*13; // K = 1/ACD_step = 1/0.08 = 12.5 ~ 13
    //ESP_LOGD("buck", "CIV with DV=%.7f dPWM= %i", (Vin-buckSetpoint.minInputVoltage), dPWM);
    
    xEventGroupClearBits(stateEventGroupHandle, (STATE_BUCK_MODE_COC | STATE_BUCK_MODE_COV | STATE_BUCK_MODE_SAFED));
    xEventGroupSetBits(stateEventGroupHandle, (STATE_BUCK_MODE_CIV));
    
  }else if(Vout > buckSetpoint.maxOutputVoltage){ // COV mode
    if(unlikely(Vout-buckSetpoint.maxOutputVoltage > 2.0f)){
      ESP_LOGD("buck", "triggered output overvoltage Vout=%.4f >> %.4f", Vout, buckSetpoint.maxOutputVoltage);
      return false;
    }
    dPWM = (buckSetpoint.maxOutputVoltage-Vout)*20.5/2; // K = 1/ACD_step = 1/0.049 = 20.408 ~ 20.5
    //ESP_LOGD("buck", "COV with DV=%.7f dPWM= %i", (buckSetpoint.maxOutputVoltage-Vout), dPWM);
    
    xEventGroupClearBits(stateEventGroupHandle, (STATE_BUCK_MODE_CIV | STATE_BUCK_MODE_COC | STATE_BUCK_MODE_SAFED));
    xEventGroupSetBits(stateEventGroupHandle, (STATE_BUCK_MODE_COV));
    
  }else if(Iout > buckSetpoint.maxOutputCurrent){ // COC mode
    if(unlikely(Iout-buckSetpoint.maxOutputCurrent > 1.0f)){
      ESP_LOGD("buck", "triggered output overcurrent Iout=%.4f >> %.4f", Iout, buckSetpoint.maxOutputCurrent);
      return false;
    }
    dPWM = (buckSetpoint.maxOutputCurrent-Iout)*12; // K = 1/ACD_step = 1/0.0833 = 12
    //ESP_LOGD("buck", "COC with DC=%.7f dPWM= %i", (buckSetpoint.maxOutputCurrent-Iout), dPWM);
    
    xEventGroupClearBits(stateEventGroupHandle, (STATE_BUCK_MODE_CIV | STATE_BUCK_MODE_COV | STATE_BUCK_MODE_SAFED));
    xEventGroupSetBits(stateEventGroupHandle, ( STATE_BUCK_MODE_COC));
    
  }
  pwmPerturb(dPWM);
  //ESP_LOGD("buck", "Buck running at pwmHS=%i pwmLS=%i", pwmHS, pwmLS);
  
  return true;
}

bool safingLoop(){
  float Vin = sensorReadings.inputVoltage;
  float Vout = sensorReadings.outputVoltage;
  float Iout = sensorReadings.outputCurrent;
  
  
  if (Vin < buckSetpoint.minInputVoltage){ // CIV mode
    //ESP_LOGD("buck", "Buck safed undervoltage (input)");
    return true;
  }else if(Vout > buckSetpoint.maxOutputVoltage){ // COV mode
    //ESP_LOGD("buck", "Buck safed overvoltage (output)");
    return true;
  }else if(Iout > buckSetpoint.maxOutputCurrent){ // COC mode
    //ESP_LOGD("buck", "Buck safed overcurrent (output)");
    return true;
  }
  ESP_LOGD("buck", "Buck safe released");
  return false;
}

bool waitForSensorReadings(){
  EventBits_t uxBits = xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READ_EVENT, true, true, sensorTimeoutTicks);
  return uxBits & (STATE_SENSORS_READ_EVENT | STATE_BUCK_ENABLED) == (STATE_SENSORS_READ_EVENT | STATE_BUCK_ENABLED);
}

void buckTask(void * parameter){
  
  xEventGroupWaitBits(stateEventGroupHandle, STATE_SENSORS_READY, false, true, portMAX_DELAY);
  for(;;){
    disable();
    xEventGroupClearBits(stateEventGroupHandle, (STATE_BUCK_MODE_CIV | STATE_BUCK_MODE_COC | STATE_BUCK_MODE_COV | STATE_BUCK_MODE_SAFED));
    xEventGroupWaitBits(stateEventGroupHandle, STATE_BUCK_ENABLED, false, true, portMAX_DELAY);
    
    xEventGroupSetBits(stateEventGroupHandle, (STATE_BUCK_MODE_SAFED));
    while(waitForSensorReadings() && safingLoop()){}
    xEventGroupClearBits(stateEventGroupHandle, (STATE_BUCK_MODE_SAFED));
    while(waitForSensorReadings() && buckControllLoop()){}
    ESP_LOGI("buck", "buckTask looped");
  }
}
