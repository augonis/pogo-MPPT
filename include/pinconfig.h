#pragma once
#include <cstdint>
#include <Esp.h>
#include "driver/adc.h"

enum class PinConfig  : uint8_t {
    I2C_SDA = 21,
    I2C_SCL = 22,
    ADC_ALERT = 4,

    Bridge_IN = 17,
    Bridge_EN = 16,
    Backflow_EN = 18,

    LED = 2,
    NTC = 35,
    Fan = 0,


    //buttonLeft = 18    ,
    //buttonRight = 17   ,
    //buttonBack =19   ,
    //buttonSelect =23 ,

    //ADC_Vin = ADC1_CHANNEL_3,
    //ADC_Vout = ADC1_CHANNEL_6,
    //ADC_Iin = ADC1_CHANNEL_0,
};

