#include <ESP32ULPDimmer.h>


int inc = 10;
int duty = 0;

void setup() {

  Serial.begin(115200);
  while (!Serial);
  Serial.println("start");

  while (ULPDimmerBegin(25, 26, 60)) {
    Serial.println("Invalid rtc_io pins!");
    delay(100);
  }
  
  inc = ULPDimmerGetStepResolution() + 0.5;
  
  ULPDimmerSetDuty(50);
}


void loop() {

  ULPDimmerSetDuty(duty);

  Serial.print("Duty=");
  Serial.println(ULPDimmerGetDuty());

  duty += inc;

  //change increment direction
  if ((duty == 100) || (duty == 0 ))inc = -inc;

  delay(500);
}
