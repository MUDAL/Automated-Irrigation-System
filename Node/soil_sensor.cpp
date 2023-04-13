#include <Arduino.h>
#include "soil_sensor.h"

SoilSensor::SoilSensor(uint8_t sensPin,uint8_t numOfSamples)
{
  this->sensPin = sensPin;
  if(numOfSamples < MAX_SAMPLES)
  {
    this->numOfSamples = numOfSamples;  
  }
  else
  {
    this->numOfSamples = MAX_SAMPLES;
  }
}

uint8_t SoilSensor::GetMoisture(void)
{
  const uint16_t rawDataInAir = 450; //0% moisture
  const uint16_t rawDataInWater = 180; //100% moisture
  uint32_t averageRawData = 0;
  //Get average raw sensor data (for better accuracy)
  for(uint8_t i = 0; i < numOfSamples; i++)
  {
    averageRawData += analogRead(this->sensPin);
  }
  averageRawData = lround((float)averageRawData / numOfSamples);
  //Prevent raw data from getting out of bounds
  if(averageRawData > rawDataInAir)
  {
    averageRawData = rawDataInAir;
  }
  if(averageRawData < rawDataInWater)
  {
    averageRawData = rawDataInWater;
  }
  //Convert raw data to percent moisture by linear interpolation
  float num = (float)averageRawData - (float)rawDataInWater;
  float denom = (float)rawDataInWater - (float)rawDataInAir;
  return lround(100.0 + (100.0 * num / denom));
}
