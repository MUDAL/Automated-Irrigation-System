#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_Sensor.h> //Version 1.0.2
#include <Adafruit_BME280.h> //Version 1.0.7
#include "hc12.h"
#include "soil_sensor.h"

typedef struct 
{
  uint8_t minMoist;
  uint8_t maxMoist;
  uint8_t minIrrigTime;
  uint8_t maxIrrigTime;
  uint8_t currentHour;
  uint8_t currentMinute;  
  uint8_t forecastHour;
  uint8_t forecastMinute;
  uint8_t probOfPrecip;
  uint8_t irrigCmd;
}masterData_t;

const uint8_t hc12Tx = 2;
const uint8_t hc12Rx = 3;
const uint8_t sValve = 4;
const uint8_t trigPin = 9;
const uint8_t echoPin = 10;
const uint8_t battPin = A1;

//Water levels of tank used [Depends on type of container used]
//[A container with a height in the range of 24-27cm is used]
const uint8_t lowWaterLevel = 20; // >= 

SoftwareSerial hc12Serial(hc12Tx,hc12Rx);
HC12 hc12(&hc12Serial);
SoilSensor soilSens(A0);
Adafruit_BME280 bme;
uint32_t prevTime = 0;

/**
 * Get distance (in cm) measured by sensor.
 * Call periodically to prevent inaccurate readings.
*/
static uint16_t GetDistance(void)
{
  //Sensor trigger
  digitalWrite(trigPin,HIGH);
  delayMicroseconds(15);
  digitalWrite(trigPin,LOW);
  uint32_t pulseWidth = pulseIn(echoPin,HIGH);
  uint16_t distance = lround(pulseWidth / 58.0);
  return distance;
}

/**
 * @brief Deactivate the solenoid valve if water level is low
*/
static void ShutValveIfLevelIsLow(bool& isWaterLevelLow,bool& isValveOn)
{
  if(isWaterLevelLow && isValveOn)
  {
    digitalWrite(sValve,LOW);
    isValveOn = false;
  }  
}

/**
 * @brief Handle irrigation based on probability of precipitation
*/
static void IrrigateViaPoP(masterData_t& masterData,uint16_t& distance)
{
  static bool isValveOn;
  bool isWaterLevelLow = (distance >= lowWaterLevel);
  
  bool isMinIrrig = (masterData.probOfPrecip >= 50) && 
                    (masterData.probOfPrecip < 70)  &&
                    (masterData.currentHour == masterData.forecastHour) &&
                    (masterData.currentMinute >= masterData.forecastMinute) &&
                    (masterData.currentMinute < (masterData.forecastMinute + masterData.minIrrigTime));
  
  bool isMaxIrrig = (masterData.probOfPrecip < 50) && 
                    (masterData.currentHour == masterData.forecastHour) &&
                    (masterData.currentMinute >= masterData.forecastMinute) &&
                    (masterData.currentMinute < (masterData.forecastMinute + masterData.maxIrrigTime));
                    
  if((isMinIrrig || isMaxIrrig) && !isValveOn)
  {
    digitalWrite(sValve,HIGH);
    isValveOn = true;
  }

  if(!isMinIrrig && !isMaxIrrig && isValveOn)
  {
    digitalWrite(sValve,LOW);
    isValveOn = false;
  }  
  ShutValveIfLevelIsLow(isWaterLevelLow,isValveOn);
}

/**
 * @brief Handle irrigation based on MQTT command
*/
static void IrrigateViaMqtt(masterData_t& masterData,uint16_t& distance)
{
  static bool isValveOn;
  const uint8_t valveOffCmd = 0;
  const uint8_t valveOnCmd = 1;
  bool isWaterLevelLow = (distance >= lowWaterLevel);
  
  switch(masterData.irrigCmd)
  {
    case valveOffCmd:
      digitalWrite(sValve,LOW);
      isValveOn = false;
      break;
    case valveOnCmd:
      if(!isWaterLevelLow)
      {
        digitalWrite(sValve,HIGH);
        isValveOn = true;
      }
      break;
  }
  ShutValveIfLevelIsLow(isWaterLevelLow,isValveOn);
}

/**
 * @brief Handle evening irrigation [via sensor]
*/
static void IrrigateViaSensor(masterData_t& masterData,uint16_t& distance,uint8_t& moisture)
{
  static uint8_t startingIrrigMinute;
  static bool isValveOn;
  static bool isMoistureLow;
  static bool isMoistureMid;
  bool isWaterLevelLow = (distance >= lowWaterLevel);
  //Start irrigation at appropriate time [6pm]
  bool isTimeForIrrig = (masterData.currentHour == 18) &&
                        (masterData.currentMinute == 0);
                        
  bool isMaxIrrigDone = isValveOn && isMoistureLow && 
                        (masterData.currentMinute >= (startingIrrigMinute + masterData.maxIrrigTime));
                        
  bool isMinIrrigDone =  isValveOn && isMoistureMid &&
                        (masterData.currentMinute >= (startingIrrigMinute + masterData.minIrrigTime));
                        
  if(isTimeForIrrig && !isValveOn)
  {
    isMoistureLow = moisture <= masterData.minMoist;
    isMoistureMid = (moisture > masterData.minMoist) && (moisture < masterData.maxMoist);
    if(isMoistureLow || isMoistureMid)
    {
      startingIrrigMinute = masterData.currentMinute;
      digitalWrite(sValve,HIGH);
      isValveOn = true; 
    }
  }
  //Stopping irrigation using tank's water level and programmed irrigation duration
  if(isMaxIrrigDone || isMinIrrigDone)
  {
    digitalWrite(sValve,LOW);
    isValveOn = false;
  }
  ShutValveIfLevelIsLow(isWaterLevelLow,isValveOn);
}

/**
 * Get battery level (or voltage)
 * For the battery voltage calculation, a voltage divider with 2k and 1k
 * resistors was used. The 1k resistor is connected to ground hence the 
 * output is approximately battery voltage / 3.
 * 
 * Calculations:
 * - Minimum battery voltage = 10.2;  //3 batteries (approx. 3.40V for each)
 * - Maximum battery voltage = 12.45; //3 batteries (approx. 4.15V for each)
 * 
 * Return battery level in encoded form (i.e. battery voltage * 10). The master 
 * must decode the battery level by dividing this encoded data by 10.
*/
static uint8_t GetBatteryLevel(void)
{
  const uint8_t numOfSamples = 600;
  uint32_t averageADC = 0;
  int32_t battLevel = 0;

  for(uint8_t i = 0; i < numOfSamples; i++)
  {
    averageADC += analogRead(battPin);
  }
  averageADC = lround((float)averageADC / numOfSamples);
  
  float voltDivOutput = 5 * (averageADC / 1024.0);
  float battVoltage = voltDivOutput * 3;
  //Encode battery level(or voltage)
  battLevel = lround(10 * battVoltage);
  return battLevel;
}

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  if(bme.begin(0x76)) 
  {
    Serial.println("BME280 found");
  }
  pinMode(sValve,OUTPUT); //solenoid valve pin init
  digitalWrite(sValve,LOW);
  //Ultrasonic sensor
  pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
  prevTime = millis();
}

void loop() 
{
  // put your main code here, to run repeatedly:
  static masterData_t masterData;
  
  if(hc12.ReceivedData())
  {
    if(hc12.DecodeData(HC12::RxDataId::DATA_QUERY) == HC12::QUERY)
    {
      Serial.println("Received data: ");
      masterData.minMoist = hc12.DecodeData(HC12::RxDataId::MIN_MOIST);
      masterData.maxMoist = hc12.DecodeData(HC12::RxDataId::MAX_MOIST);
      masterData.minIrrigTime = hc12.DecodeData(HC12::RxDataId::MIN_IRRIG_TIME);
      masterData.maxIrrigTime = hc12.DecodeData(HC12::RxDataId::MAX_IRRIG_TIME);
      masterData.forecastHour = hc12.DecodeData(HC12::RxDataId::FORECAST_HOUR);
      masterData.forecastMinute = hc12.DecodeData(HC12::RxDataId::FORECAST_MIN);
      masterData.probOfPrecip = hc12.DecodeData(HC12::RxDataId::PROB_OF_PREC);
      masterData.irrigCmd = hc12.DecodeData(HC12::RxDataId::IRRIG_CMD);
      masterData.currentHour = hc12.DecodeData(HC12::RxDataId::CURRENT_HOUR);
      masterData.currentMinute = hc12.DecodeData(HC12::RxDataId::CURRENT_MINUTE);
      
      Serial.println(masterData.minMoist);
      Serial.println(masterData.maxMoist);
      Serial.println(masterData.minIrrigTime);
      Serial.println(masterData.maxIrrigTime);
      Serial.println(masterData.currentHour);
      Serial.println(masterData.currentMinute);      
      Serial.println(masterData.forecastHour);
      Serial.println(masterData.forecastMinute);
      Serial.println(masterData.probOfPrecip);
      Serial.println(masterData.irrigCmd);
      Serial.println();

      uint8_t moisture = soilSens.GetMoisture();
      uint8_t temperature = lround(bme.readTemperature()); 
      uint8_t humidity = lround(bme.readHumidity());
      uint16_t distance = GetDistance();
      bool isWaterLevelLow = (distance >= lowWaterLevel);
      uint8_t battLevel = GetBatteryLevel();

      IrrigateViaMqtt(masterData,distance);
      
      hc12.EncodeData(HC12::ACK,HC12::TxDataId::DATA_ACK);
      hc12.EncodeData(moisture,HC12::TxDataId::NODE_MOIST);
      hc12.EncodeData(temperature,HC12::TxDataId::NODE_TEMP);
      hc12.EncodeData(humidity,HC12::TxDataId::NODE_HUMID);
      hc12.EncodeData(!isWaterLevelLow,HC12::TxDataId::NODE_WL);
      hc12.EncodeData(battLevel,HC12::TxDataId::NODE_BL);
      
      Serial.println("Transmitting data to master");
      hc12.TransmitData();
    }
  }
  
  if((millis() - prevTime) >= 150)
  {
    uint8_t moisture = soilSens.GetMoisture();
    uint16_t distance = GetDistance();
    IrrigateViaSensor(masterData,distance,moisture); 
    IrrigateViaPoP(masterData,distance);
    prevTime = millis();
  }
}
