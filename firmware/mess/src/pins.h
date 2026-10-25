// Pinbelegung laut docs/HARDWARE_KONZEPT.md, Kap. 5
#pragma once

#include "hal/adc_types.h"

#define PIN_ENV_LOW     1   // Hüllkurve LOW  (ADC1_CH0)
#define PIN_ENV_HIGH    2   // Hüllkurve HIGH (ADC1_CH1)
#define PIN_VBAT        4   // Akku über Teiler 1:2 (ADC1_CH3)
#define PIN_ANALOG_EN   6   // Low = Analogteil versorgt (P-MOSFET)

#define PIN_BUZ_IN1     39  // DRV8833 AIN1
#define PIN_BUZ_IN2     40  // DRV8833 AIN2
#define PIN_BUZ_SLEEP   41  // DRV8833 nSLEEP
#define PIN_BOOST_EN    42  // High-Side-Schalter MT3608

#define ADC_CH_LOW      ADC_CHANNEL_0
#define ADC_CH_HIGH     ADC_CHANNEL_1
#define ADC_CH_VBAT     ADC_CHANNEL_3
