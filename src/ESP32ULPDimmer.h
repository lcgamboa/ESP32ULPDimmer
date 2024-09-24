/**
  ESP32 ULP Dimmer

  Copyright (c) : 2024  Luis Claudio Gambôa Lopes <lcgamboa@yahoo.com>
  Released under the MIT license.
*/   
   
#ifndef  _ESP32ULPDIMMER_
#define _ESP32ULPDIMMER_    

int ULPDimmerBegin(const int zc_pin, const int trigger_pin, const  unsigned int cycle_freq); // cycle_freq  is 50 or 60Hz
void ULPDimmerSetDuty(unsigned int duty); // 0 to 100%
unsigned int ULPDimmerGetDuty(void); // 0 to 100%
float ULPDimmerGetStepResolution(void); // in %

#endif
