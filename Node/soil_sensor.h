#pragma once

#define MAX_SAMPLES  20

class SoilSensor
{
  private:
    uint8_t sensPin;
    uint8_t numOfSamples;
  public:
    SoilSensor(uint8_t sensPin,uint8_t numOfSamples = MAX_SAMPLES);
    uint8_t GetMoisture(void);
};
