/**
  ESP32 ULP Dimmer

  Copyright (c) : 2024  Luis Claudio Gambôa Lopes <lcgamboa@yahoo.com>
  Released under the MIT license.
*/ 
   
#include "esp32/ulp.h"
#include "driver/rtc_io.h"
#include "ESP32ULPDimmer.h"
#include <string.h>

//pulse time calculation
//trigger time =  (set_point * 0.2) + 0.6 in ms

// Slow memory variable assignment
enum {
  SLOW_INPUT_STATE,     // last input state
  SLOW_SET_POINT,       // duty cycle
  SLOW_TRIGGER,         // Trigger status
  SLOW_PROG_ADDR,       // Program start address
};

static int time_limit = 0;

int ULPDimmerBegin(const int zc_pin, const int trigger_pin, const unsigned int cycle_freq) {

  if (!(rtc_gpio_is_valid_gpio((gpio_num_t)zc_pin) && rtc_gpio_is_valid_gpio((gpio_num_t)trigger_pin))) {
    return 1;
  }


  time_limit =  ((1000.0 / (2.0 * cycle_freq) - 0.6 ) / 0.2) + 2.0;

  // Set ULP activation interval
  ulp_set_wakeup_period(0, 200);

  // Slow memory initialization
  memset(RTC_SLOW_MEM, 0, 8192);

  //status initialization
  RTC_SLOW_MEM[SLOW_INPUT_STATE] = 0;
  RTC_SLOW_MEM[SLOW_SET_POINT] = 10; // trigger time = 2.7 ms [(10 * 0.2) + 0.6 in ms]

  RTC_SLOW_MEM[SLOW_TRIGGER] = 0;


  // PIN to out (specify by +14)
  const uint32_t pin_out_bit = rtc_io_number_get((gpio_num_t)trigger_pin) + 14;
  const gpio_num_t pin_out = (gpio_num_t)trigger_pin;

  // GPIO initialization (set to output and initial value is 0)
  rtc_gpio_init(pin_out);
  rtc_gpio_set_direction(pin_out, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level(pin_out, 0);

  // PIN to in (specify by +14)
  const uint32_t pin_in_bit = rtc_io_number_get((gpio_num_t)zc_pin) + 14;
  const gpio_num_t pin_in = (gpio_num_t)zc_pin;
  //INPUT PIN
  rtc_gpio_init(pin_in);
  rtc_gpio_set_direction(pin_in, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_dis(pin_in);
  rtc_gpio_pullup_dis(pin_in);
  rtc_gpio_hold_en(pin_in);

  // ULP Program
  const ulp_insn_t  ulp_prog[] = {
    //Read atual input pin to R2
    I_RD_REG(RTC_GPIO_IN_REG, pin_in_bit, pin_in_bit),// R0 = pin_in_bit
    I_MOVR(R2, R0),                         // R2 = R0
    //Read last input to R0
    I_MOVI(R3, SLOW_INPUT_STATE),           // R3 = SLOW_INPUT_STATE
    I_LD(R0, R3, 0),                        // R0 = RTC_SLOW_MEM[R3(SLOW_INPUT_STATE)]
    //Update last input with atual input (R2)
    I_ST(R2, R3, 0),                        // RTC_SLOW_MEM[R3(SLOW_INPUT_STATE)] = R2
    //Detect down edge between atual input (R2) with last input (R0)
    I_ADDR(R0, R0, R2),                     // R0 = R0 + R2
    I_ANDI(R0, R0, R2),                      // R0 = R0 AND 1
    M_BL(1, 1),                             // IF R0 < 1 THAN GOTO M_LABEL(1)
    // SLOW_TRIGGER = 0 R1 counter = 0
    I_MOVI(R1, 0),                          // R1 = 0
    I_MOVI(R3, SLOW_TRIGGER),               // R3 = SLOW_TRIGGER
    I_ST(R1, R3, 0),                        // RTC_SLOW_MEM[R3(SLOW_TRIGGER)] = R1

    M_LABEL(1),                             // M_LABEL(1)
    //Read SET_POINT
    I_MOVI(R3, SLOW_SET_POINT),             // R3 = SLOW_SET_POINT
    I_LD(R0, R3, 0),                        // R0 = RTC_SLOW_MEM[R3(SLOW_SET_POINT)]
    // Compare SET_POINT With R1 counter
    I_SUBR(R0, R1, R0),                     // R0 = R1 -R0
    M_BXF(2),                               // IF OVERFLOW THAN GOTO M_LABEL(2)

    I_MOVI(R3, SLOW_TRIGGER),               // R3 = SLOW_TRIGGER
    I_LD(R0, R3, 0),                        // R0 = RTC_SLOW_MEM[R3(SLOW_TRIGGER)]
    //Verify if SLOW_TRIGGER >= 1
    M_BGE(2, 1),                            // IF R0 >= 1 THAN GOTO M_LABEL(2)

    //Enable output
    I_WR_REG(RTC_GPIO_OUT_REG, pin_out_bit, pin_out_bit, 1), // pin_out_bit = 1
    // SLOW_TRIGGER = R1 counter
    I_MOVI(R3, SLOW_TRIGGER),               // R3 = SLOW_TRIGGER
    I_ST(R1, R3, 0),                        // RTC_SLOW_MEM[R3(SLOW_TRIGGER)] = R1
    I_DELAY(50),

    //End
    M_LABEL(2),                             // M_LABEL(2)
    //Disable output
    I_WR_REG(RTC_GPIO_OUT_REG, pin_out_bit, pin_out_bit, 0),// pin_out_bit = 0
    //Increment R1 counter
    I_ADDI(R1, R1, 1),                      // R1++
    I_HALT()                                // Stop the program
  };

  // Run the program shifted backward by the number of variables
  size_t size = sizeof(ulp_prog) / sizeof(ulp_insn_t);

  ulp_process_macros_and_load(SLOW_PROG_ADDR, ulp_prog, &size);
  ulp_run(SLOW_PROG_ADDR);

  return 0;
}


void ULPDimmerSetDuty(unsigned int duty) {

  if (duty > 100) duty = 100;

  duty = 100 - duty;

  duty = (duty * time_limit) / 100;

  RTC_SLOW_MEM[SLOW_SET_POINT] = duty;

}

unsigned int ULPDimmerGetDuty(void) {
  return  100 - ((RTC_SLOW_MEM[SLOW_SET_POINT] * 100) / time_limit);
}

float ULPDimmerGetStepResolution(void) {
  return 100.0 / time_limit;
}




