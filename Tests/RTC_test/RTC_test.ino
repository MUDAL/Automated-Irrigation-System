#include <Wire.h>
#include "RTClib.h" //Version 1.3.3

static RTC_DS3231 rtc;

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  if(!rtc.begin())
  {
    Serial.println("Could not find RTC");
  }
  //Set date and time: year,month,day,hour,minute,second
  //rtc.adjust(DateTime(2022,12,7,20,49,0)); 
}

void loop() 
{
  // put your main code here, to run repeatedly:
  DateTime dateTime = rtc.now();
  uint8_t hour = dateTime.hour();
  uint8_t minute = dateTime.minute();
  Serial.print("hour = ");
  Serial.println(hour);
  Serial.print("minute = ");
  Serial.println(minute);
  delay(2000);
}
