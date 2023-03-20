#include <Arduino.h>
#include <string.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include "keypad.h"
#include "RTClib.h" //Version 1.3.3
#include "hmi.h"

//LCD (indexes and number of rows)
const uint8_t minRow = 0;
const uint8_t maxRow = 1;
const uint8_t numOfRows = 2;

/**
 * @brief Converts a string to an 8-bit integer.
*/
void StringTo8BitInteger(char* stringPtr,uint8_t* integerPtr)
{
  *integerPtr = 0;
  uint8_t len = strlen(stringPtr);
  uint32_t j = 1;
  for(uint8_t i = 0; i < len; i++)
  {
    *integerPtr += ((stringPtr[len - i - 1] - '0') * j);
    j *= 10;
  }
}

void HMI::AlignData(uint8_t param)
{
  if(param < 10)
  {
    lcdPtr->print('0');
  }
  lcdPtr->print(param);
}

void HMI::StoreKey(char key,char* keyBuffer,uint8_t len)
{
  for(uint8_t i = 0; i < len - 1 ;i++)
  {
    keyBuffer[i] = keyBuffer[i + 1];
  }
  keyBuffer[len - 1] = key;   
}

void HMI::SetParam(uint8_t* paramPtr,char* paramBuffer,uint8_t paramColumn)
{
  while(1)
  {
    char key = keypadPtr->GetChar();
    if(isdigit(key))
    {
      HMI::StoreKey(key,paramBuffer,2);
      StringTo8BitInteger(paramBuffer,paramPtr);
      lcdPtr->setCursor(paramColumn,currentRow);
      HMI::AlignData(*paramPtr);
    }
    else
    {
      if(key == '#')
      {
        break;
      }
    }
  }
}

void HMI::PointToRow(char* heading1,char* heading2,uint8_t row)
{
  char* heading[] = {heading1,heading2};
  for(uint8_t i = 0; i < numOfRows; i++)
  {
    if(row == i)
    {
      heading[i][0] = '-'; //highlight current row
    }
    lcdPtr->setCursor(0,i);
    lcdPtr->print(heading[i]);
  }  
}

void HMI::ChangeStateTo(State nextState)
{
  currentState = nextState;
  lcdPtr->clear();
}

void HMI::StateFunc_Moisture(void)
{
  uint8_t* moistPtr[] = {&minMoist,&maxMoist};
  char* moistBuff[] = {minMoistBuff,maxMoistBuff};
  char heading1[] = "  MinMoist:";
  char heading2[] = "  MaxMoist:";
  //columns where data display begins
  uint8_t dataCol[2] = {0};
  dataCol[0] = strlen(heading1) + 1;
  dataCol[1] = strlen(heading2) + 1;
  
  HMI::PointToRow(heading1,heading2,currentRow);
  for(uint8_t i = 0; i < numOfRows; i++)
  {
    lcdPtr->setCursor(dataCol[i],i);
    HMI::AlignData(*(moistPtr[i])); 
    lcdPtr->print('%');
  }
      
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      HMI::ChangeStateTo(ST_IRRIG_TIME);
      break;
    case 'C':
      currentRow = minRow;
      break;
    case 'D':
      currentRow = maxRow;
      break;
    case '#':
      lcdPtr->setCursor(1,currentRow);
      lcdPtr->print('>');
      HMI::SetParam(moistPtr[currentRow],moistBuff[currentRow],dataCol[currentRow]);
      break;
    case '*':
      saveButtonPressed = true;
      break;      
  }
}

void HMI::StateFunc_IrrigTime(void)
{
  uint8_t* irrigPtr[] = {&minIrrigTime,&maxIrrigTime};
  char* irrigBuff[] = {minIrrigBuff,maxIrrigBuff};
  char heading1[] = "  MinIRG:";
  char heading2[] = "  MaxIRG:";
  //columns where data display begins
  uint8_t dataCol[2] = {0};
  dataCol[0] = strlen(heading1) + 1;
  dataCol[1] = strlen(heading2) + 1;
  
  HMI::PointToRow(heading1,heading2,currentRow);
  for(uint8_t i = 0; i < numOfRows; i++)
  {
    lcdPtr->setCursor(dataCol[i],i);
    HMI::AlignData(*(irrigPtr[i]));
    lcdPtr->print("min");  
  }  
  
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      HMI::ChangeStateTo(ST_FORECAST);
      break;
    case 'B':
      HMI::ChangeStateTo(ST_MOISTURE);
    case 'C':
      currentRow = minRow;
      break;
    case 'D':
      currentRow = maxRow;
      break;
    case '#':
      lcdPtr->setCursor(1,currentRow);
      lcdPtr->print('>');
      HMI::SetParam(irrigPtr[currentRow],irrigBuff[currentRow],dataCol[currentRow]);
      break;
    case '*':
      saveButtonPressed = true;
      break;
  }
}

void HMI::StateFunc_Forecast(void)
{
  uint8_t* forecastPtr[] = {&forecastHour,&forecastMinute};
  char* forecastBuff[] = {forecastHrBuff,forecastMinBuff};
  char heading1[] = "  FcstHr:";
  char heading2[] = "  FcstMin:"; 
  //columns where data display begins
  uint8_t dataCol[2] = {0};
  dataCol[0] = strlen(heading1) + 2;
  dataCol[1] = strlen(heading2) + 1;
   
  HMI::PointToRow(heading1,heading2,currentRow);
  for(uint8_t i = 0; i < numOfRows; i++)
  {
    const uint8_t mod[] = {24,60}; //mod: hour,minute
    lcdPtr->setCursor(dataCol[i],i);
    *(forecastPtr[i]) %= mod[i];
    HMI::AlignData(*(forecastPtr[i]));  
  }
      
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      HMI::ChangeStateTo(ST_POP);
      break;    
    case 'B':
      HMI::ChangeStateTo(ST_IRRIG_TIME);
      break;
    case 'C':
      currentRow = minRow;
      break;
    case 'D':
      currentRow = maxRow;
      break;
    case '#':
      lcdPtr->setCursor(1,currentRow);
      lcdPtr->print('>');
      HMI::SetParam(forecastPtr[currentRow],forecastBuff[currentRow],dataCol[currentRow]);
      break;
    case '*':
      saveButtonPressed = true;
      break;      
  }
}

void HMI::StateFunc_PoP(void)
{
  lcdPtr->setCursor(0,0);
  lcdPtr->print("3pm Forecast");
  lcdPtr->setCursor(0,1);   
  lcdPtr->print("Prob.Rain: ");
  lcdPtr->print(probOfPrecip); 
  //2 spaces after % for proper display when number of digits change
  lcdPtr->print("%  "); 
  
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      HMI::ChangeStateTo(ST_BATT_LEVEL);
      break;   
    case 'B':
      HMI::ChangeStateTo(ST_FORECAST);
      break;
    case '*':
      saveButtonPressed = true;
      break;      
  }  
}

void HMI::StateFunc_BattLevel(void)
{
  lcdPtr->setCursor(0,0);
  lcdPtr->print("BattNode: ");
  lcdPtr->print((battLevel / 10.0),1); //display decoded battery level
  //2 spaces after V for proper display when number of digits change
  lcdPtr->print("V  "); 
  lcdPtr->setCursor(0,1);
  lcdPtr->print("Low if <= 10.2V");
  
  char key = keypadPtr->GetChar();
  switch(key)
  { 
    case 'A':
      HMI::ChangeStateTo(ST_TIME);
      break;  
    case 'B':
      HMI::ChangeStateTo(ST_POP);
      break;
    case '*':
      saveButtonPressed = true;
      break;      
  }    
}

void HMI::StateFunc_DisplayTime(void)
{
  DateTime dateTime = rtcPtr->now();
  uint8_t currentHour = dateTime.hour();
  uint8_t currentMin = dateTime.minute();
  uint8_t currentSec = dateTime.second();
  lcdPtr->setCursor(0,0);
  lcdPtr->print("Clock time: ");
  lcdPtr->setCursor(0,1);
  HMI::AlignData(currentHour);
  lcdPtr->print(':');
  HMI::AlignData(currentMin);
  lcdPtr->print(':');
  HMI::AlignData(currentSec);
  
  char key = keypadPtr->GetChar();
  switch(key)
  {   
    case 'B':
      HMI::ChangeStateTo(ST_BATT_LEVEL);
      break;
    case '*':
      saveButtonPressed = true;
      break;      
  }   
}

HMI::HMI(LiquidCrystal_I2C* lcdPtr,Keypad* keypadPtr)
{
  //Initialize private variables
  this->lcdPtr = lcdPtr;
  this->keypadPtr = keypadPtr;
  currentState = ST_MOISTURE;
  currentRow = minRow;
  saveButtonPressed = false;
  probOfPrecip = 0;
  battLevel = 0;  
  signalFlag = false;
  
  minMoist = 0;
  maxMoist = 0;
  minIrrigTime = 0;
  maxIrrigTime = 0;
  forecastHour = 0;
  forecastMinute = 0; 

  uint8_t i;
  for(i = 0; i < 2; i++)
  {
    minMoistBuff[i] = '0';
    maxMoistBuff[i] = '0';
    minIrrigBuff[i] = '0';
    maxIrrigBuff[i] = '0';
    forecastHrBuff[i] = '0';
    forecastMinBuff[i] = '0'; 
  } 
  minMoistBuff[i] = '\0';
  maxMoistBuff[i] = '\0';
  minIrrigBuff[i] = '\0';
  maxIrrigBuff[i] = '\0';
  forecastHrBuff[i] = '\0';
  forecastMinBuff[i] = '\0';  
}

void HMI::RegisterClock(RTC_DS3231* rtcPtr)
{
  this->rtcPtr = rtcPtr;
}

void HMI::Start(void)
{
  switch(currentState)
  {
    case ST_MOISTURE:
      HMI::StateFunc_Moisture();
      break;
    case ST_IRRIG_TIME:
      HMI::StateFunc_IrrigTime();
      break;
    case ST_FORECAST:
      HMI::StateFunc_Forecast();
      break; 
    case ST_POP:
      HMI::StateFunc_PoP();
      break;  
    case ST_BATT_LEVEL:
      HMI::StateFunc_BattLevel();
      break;     
    case ST_TIME:
      HMI::StateFunc_DisplayTime();
      break; 
  }
}

uint8_t HMI::GetParamConfig(Config paramConfig)
{
  uint8_t setting; //param config or setting
  switch(paramConfig)
  {
    case MIN_MOIST:
      setting = minMoist;
      break;
    case MAX_MOIST:
      setting = maxMoist;
      break;
    case MIN_IRRIG_TIME:
      setting = minIrrigTime;
      break;
    case MAX_IRRIG_TIME:
      setting = maxIrrigTime;
      break;
    case FORECAST_HOUR:
      setting = forecastHour;
      break;
    case FORECAST_MIN:
      setting = forecastMinute;
      break;
  }
  return setting;
}

void HMI::SetParamConfig(Config paramConfig,uint8_t setting)
{
  switch(paramConfig)
  {
    case MIN_MOIST:
      minMoist = setting;
      break;
    case MAX_MOIST:
      maxMoist = setting;
      break;
    case MIN_IRRIG_TIME:
      minIrrigTime = setting;
      break;
    case MAX_IRRIG_TIME:
      maxIrrigTime = setting;
      break;
    case FORECAST_HOUR:
      forecastHour = setting;
      break;
    case FORECAST_MIN:
      forecastMinute = setting;
      break;
  }  
}

bool HMI::IsSaveButtonPressed(void)
{
  bool isSaveButtonPressed = saveButtonPressed;
  saveButtonPressed = false;
  return isSaveButtonPressed;
}

void HMI::RegisterPoP(uint8_t probOfPrecip)
{
  this->probOfPrecip = probOfPrecip;
}

void HMI::RegisterBattLevel(uint8_t battLevel)
{
  this->battLevel = battLevel;
}

/**
 * @brief Toggles between display of '.' and ' ' to signify
 * continuous and successful reception of data from the node.
*/
void HMI::SignalDataReception(void)
{
  lcdPtr->setCursor(15,1);
  if(!signalFlag)
  {
    lcdPtr->print(' ');
  }
  else
  {
    lcdPtr->print('.');
  }
  signalFlag ^= true;
}

void HMI::ForecastNotification(void)
{
  lcdPtr->clear();
  lcdPtr->print("Forecast: ");
  lcdPtr->setCursor(0,1);
  lcdPtr->print("SUCCESS");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lcdPtr->clear();
}

