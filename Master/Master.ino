#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include "RTClib.h" //Version 1.3.3
#include "hc12.h"
#include "keypad.h"
#include "sim800l.h"
#include "hmi.h"

//RTOS
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE  0
#else
#define ARDUINO_RUNNING_CORE  1
#endif

//Maximum number of characters
#define SIZE_PHONE_NUM        15
#define SIZE_CITY_NAME        20
#define SIZE_COUNTRY_CODE      5
#define SIZE_API_KEY          50
#define SIZE_TOPIC            30
#define SIZE_CLIENT_ID        23

//Define textboxes for parameters to be provisioned via the captive portal
WiFiManagerParameter phoneNum("0","Phone (incl. +234)","",SIZE_PHONE_NUM);
WiFiManagerParameter city("1","City name","",SIZE_CITY_NAME);
WiFiManagerParameter country("2","Country code","",SIZE_COUNTRY_CODE);
WiFiManagerParameter weatherApiKey("3","OpenWeather.org API key","",SIZE_API_KEY);
WiFiManagerParameter pubTopic("4","HiveMQ Publish topic","",SIZE_TOPIC);
WiFiManagerParameter subTopic("5","HiveMQ Subscription topic","",SIZE_TOPIC);
WiFiManagerParameter clientID("A","MQTT client ID","",SIZE_CLIENT_ID);
Preferences preferences; //for accessing ESP32 flash memory

//Task handle(s)
TaskHandle_t wifiTaskHandle;
TaskHandle_t forecastTaskHandle;

//Shared resources
uint8_t probOfPrecip = 0;
bool forecastReceived = false;
uint8_t soilMoisture = 0;
uint8_t temperature = 0;
uint8_t humidity = 0;
uint8_t irrigCmd = 0; 

/**
 * @brief Make an HTTP GET request to the specified server
*/
static String HttpGetRequest(const char* serverName,int* httpCode) 
{
  HTTPClient http;  
  http.begin(serverName);
  *httpCode = http.GET();
  String payload = "{}"; 
  
  if(*httpCode == HTTP_CODE_OK) 
  {
    Serial.println("Successful request ");
    payload = http.getString();
  }
  else 
  {
    Serial.print("HTTP error code: ");
    Serial.println(*httpCode);
  }
  http.end();
  return payload;
}

/**
 * @brief Store new data in specified location in ESP32's 
 * flash memory if the new ones are different from the old ones.  
*/
static void StoreNewFlashData(const char* flashLoc,const char* newData,
                              const char* oldData,uint8_t dataSize)
{
  if(strcmp(newData,"") && strcmp(newData,oldData))
  {
    preferences.putBytes(flashLoc,newData,dataSize);
  }
}

/**
 * @brief Momentarily signifies storage of HMI configurable 
 * parameters in ESP32's flash memory.
*/
static void NotifyParamSave(LiquidCrystal_I2C& lcd)
{
  lcd.clear();
  lcd.print("SETTINGS SAVED");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lcd.clear();  
}

/**
 * @brief Save HMI configuration in flash memory
*/
static void HMI_SaveState(HMI& hmi,
                          const HMI::Config* param,
                          const uint8_t& numOfParams,
                          const char* flashLoc)
{
  uint8_t hmiParamValues[numOfParams] = {0};
  for(uint8_t i = 0; i < numOfParams; i++)
  {
    hmiParamValues[i] = hmi.GetParamConfig(param[i]); 
  }
  Serial.println("Storing data in flash");
  preferences.putBytes(flashLoc,hmiParamValues,numOfParams); 
}

/**
 * @brief Load HMI configuration from flash memory
*/
static void HMI_LoadState(HMI& hmi,
                          const HMI::Config* param,
                          const uint8_t& numOfParams,
                          const char* flashLoc)
{
  uint8_t hmiParamValues[numOfParams] = {0};
  preferences.getBytes(flashLoc,hmiParamValues,numOfParams); 
  for(uint8_t i = 0; i < numOfParams; i++)
  {
    hmi.SetParamConfig(param[i],hmiParamValues[i]);
  }  
}

/**
 * @brief Extract probability of precipitation (%) from  
 * weather forecast (JSON data).  
*/
static uint8_t GetPoP(JSONVar& jsonObject,uint8_t& numOfForecasts)
{
  const uint8_t dateIndex = 12;
  uint8_t forecastIndex = 0;
  uint8_t pop = 0; //probability of precipitation(P.O.P)
  String dateStr; 
  String forecastHr;
  
  for(uint8_t i = 0; i < numOfForecasts; i++)
  {
    dateStr = JSON.stringify(jsonObject["list"][i]["dt_txt"]);
    forecastHr = dateStr.substring(dateIndex,dateIndex + 2);
    if(forecastHr == "15")
    {
      Serial.print("forecast hour: ");
      Serial.println(forecastHr);
      forecastIndex = i;
      Serial.print("forecast index = ");
      Serial.println(forecastIndex);
      //JSON data -> string -> float -> percentage
      pop = (JSON.stringify(jsonObject["list"][forecastIndex]["pop"]).toFloat()) * 100;
      break;
    }
  }
  return pop;
}

/**
 * @brief Initializes a timer for periodic sending of SMS whenever 
 * a critical condition occurs (i.e. empty tank, low battery level).
 * If the conditions return to normal, the sending of SMS will be 
 * stopped.
*/
static void TriggerTextMsgTimer(bool& isLow,
                                bool* isPrevLow,
                                HC12::RxDataId rxDataId,
                                uint32_t* prevTimerValue)
{
  if(isLow && !(*isPrevLow))
  {
    *isPrevLow = true;
    *prevTimerValue = millis();
    switch(rxDataId)
    {
      case HC12::RxDataId::NODE_WL:
        Serial.println("Water level is low"); 
        break;
      case HC12::RxDataId::NODE_BL:
        Serial.println("Battery level is low"); 
        break;
    }
  }
  else if(!isLow && *isPrevLow)
  {
    *isPrevLow = false;
    switch(rxDataId)
    {
      case HC12::RxDataId::NODE_WL:
        Serial.println("Water level is normal"); 
        break;
      case HC12::RxDataId::NODE_BL:
        Serial.println("Battery level is normal"); 
        break;
    }
  }  
}

/**
 * @brief Sends SMS whenever a critical condition occurs 
 * (i.e. empty tank, low battery level).
*/
static void SendTextMsgForCriticalEvent(SIM800L& gsm,
                                        char* prevPhoneNum,
                                        bool* isPrevLow,
                                        HC12::RxDataId rxDataId,
                                        uint32_t* prevTimerValue,
                                        uint32_t msgInterval)
{
  if(*isPrevLow && ((millis() - *prevTimerValue) >= msgInterval))
  {
    preferences.getBytes("0",prevPhoneNum,SIZE_PHONE_NUM);
    switch(rxDataId)
    {
      case HC12::RxDataId::NODE_WL:
        Serial.println("Sending empty tank SMS");
        gsm.SendSMS(prevPhoneNum,"Alert: low water level!!!!!!"); 
        break;
      case HC12::RxDataId::NODE_BL:
        Serial.println("Sending low battery SMS");
        gsm.SendSMS(prevPhoneNum,"Alert: low battery level!!!!!!"); 
        break;
    }
    *prevTimerValue = millis();
  }  
}

/**
 * @brief Converts an integer to a string.
*/
static void IntegerToString(uint32_t integer,char* stringPtr)
{
  if(integer == 0)
  {  
    stringPtr[0] = '0';
    return;
  }
  uint32_t integerCopy = integer;
  uint8_t numOfDigits = 0;

  while(integerCopy > 0)
  {
    integerCopy /= 10;
    numOfDigits++;
  }
  while(integer > 0)
  {
    stringPtr[numOfDigits - 1] = '0' + (integer % 10);
    integer /= 10;
    numOfDigits--;
  }
}

void setup() 
{
  // put your setup code here, to run once:
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  preferences.begin("Auto-Irrig",false);    
  //Create tasks
  xTaskCreatePinnedToCore(WiFiManagementTask,"",7000,NULL,1,&wifiTaskHandle,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(MqttTask,"",7000,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(WeatherForecastTask,"",10000,NULL,1,&forecastTaskHandle,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(ApplicationTask,"",20000,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  vTaskSuspend(forecastTaskHandle); 
}

void loop() 
{
  // put your main code here, to run repeatedly:
}

/**
 * @brief Manages WiFi configurations (STA and AP modes). Connects
 * to an existing/saved network if available, otherwise it acts as
 * an AP in order to receive new network credentials.
*/
void WiFiManagementTask(void* pvParameters)
{
  const uint16_t accessPointTimeout = 50000; //millisecs
  static WiFiManager wm;
  WiFi.mode(WIFI_STA); 
  wm.addParameter(&phoneNum); 
  wm.addParameter(&city);
  wm.addParameter(&country);
  wm.addParameter(&weatherApiKey); 
  wm.addParameter(&pubTopic);
  wm.addParameter(&subTopic); 
  wm.addParameter(&clientID);  
  wm.setConfigPortalBlocking(false);
  wm.setSaveParamsCallback(WiFiManagerCallback);   
  //Auto-connect to previous network if available.
  //If connection fails, ESP32 goes from being a station to being an access point.
  Serial.print(wm.autoConnect("Auto-Irrig")); 
  Serial.println("-->WiFi status");   
  bool accessPointMode = false;
  uint32_t startTime = 0;    

  while(1)
  {
    wm.process();
    if(WiFi.status() != WL_CONNECTED)
    {
      if(!accessPointMode)
      {
        if(!wm.getConfigPortalActive())
        {
          wm.autoConnect("Auto-Irrig"); 
        }
        accessPointMode = true; 
        startTime = millis(); 
      }
      else
      {
        //reset after a timeframe (device shouldn't spend too long as an access point)
        if((millis() - startTime) >= accessPointTimeout)
        {
          Serial.println("\nAP timeout reached, system will restart for better connection");
          vTaskDelay(pdMS_TO_TICKS(1000));
          esp_restart();
        }
      }
    }
    else
    {
      if(accessPointMode)
      {   
        accessPointMode = false;
        Serial.println("Successfully connected, system will restart now");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
      }
    }    
  }
}

/**
 * @brief Handles bi-directional communication with the HiveMQ broker.
*/
void MqttTask(void* pvParameters)
{
  static WiFiClient wifiClient;
  static PubSubClient mqttClient(wifiClient);
  static char dataToPublish[120];
  
  char prevPubTopic[SIZE_TOPIC] = {0};
  char prevSubTopic[SIZE_TOPIC] = {0};
  char prevClientID[SIZE_CLIENT_ID] = {0};
  const char *mqttBroker = "broker.hivemq.com";
  const uint16_t mqttPort = 1883;  
  uint32_t prevTime = millis();
  
  while(1)
  {
    if(WiFi.status() == WL_CONNECTED)
    {       
      if(!mqttClient.connected())
      {
        memset(prevPubTopic,'\0',SIZE_TOPIC);
        memset(prevSubTopic,'\0',SIZE_TOPIC);
        memset(prevClientID,'\0',SIZE_CLIENT_ID);
        preferences.getBytes("4",prevPubTopic,SIZE_TOPIC);  
        preferences.getBytes("5",prevSubTopic,SIZE_TOPIC);
        preferences.getBytes("A",prevClientID,SIZE_CLIENT_ID);
        mqttClient.setServer(mqttBroker,mqttPort);
        mqttClient.setCallback(MqttCallback);
        while(!mqttClient.connected())
        {
          if(mqttClient.connect(prevClientID))
          {
            Serial.println("Connected to HiveMQ broker");
            mqttClient.subscribe(prevPubTopic);
          }
        } 
      }
      else
      {
        if((millis() - prevTime) >= 500)
        {
          char soilMoistureBuff[3] = {0};
          char temperatureBuff[3] = {0};
          char humidityBuff[3] = {0};
          
          IntegerToString(soilMoisture,soilMoistureBuff);
          IntegerToString(temperature,temperatureBuff);
          IntegerToString(humidity,humidityBuff);

          strcat(dataToPublish,"MOIST: ");
          strcat(dataToPublish,soilMoistureBuff);
          strcat(dataToPublish," %\n");
          strcat(dataToPublish,"TEMP: ");
          strcat(dataToPublish,temperatureBuff);
          strcat(dataToPublish," C\n");
          strcat(dataToPublish,"HUMID: ");
          strcat(dataToPublish,humidityBuff);
          strcat(dataToPublish," %");
          
          mqttClient.publish(prevSubTopic,dataToPublish);
          uint32_t dataLen = strlen(dataToPublish);
          memset(dataToPublish,'\0',dataLen);
          prevTime = millis();
        }
        mqttClient.loop(); //handles mqtt callback
      }
    }
  }
}

/**
 * @brief Retrieves weather forecast of location/zone specified in the   
 * captive portal setup by the ESP32 during provisioning. The weather  
 * prediction is obtained from OpenWeatherMap.org via HTTP requests.    
 * 
*/
void WeatherForecastTask(void* pvParameters)
{
  static char serverPath[300];
  char prevCity[SIZE_CITY_NAME] = {0};
  char prevCountry[SIZE_COUNTRY_CODE] = {0};
  char prevWeatherApiKey[SIZE_API_KEY] = {0}; 
  uint8_t numOfForecasts = 12;
   
  int httpCode = -1;
  String jsonBuffer;
  uint32_t prevTime = millis();
  
  while(1)
  { 
    if(WiFi.status() == WL_CONNECTED)
    { 
      //Requesting forecast from OpenWeatherMap.org
      if((millis() - prevTime) >= 10000)
      {
        Serial.println("Forecast");
        preferences.getBytes("1",prevCity,SIZE_CITY_NAME);
        preferences.getBytes("2",prevCountry,SIZE_COUNTRY_CODE);  
        preferences.getBytes("3",prevWeatherApiKey,SIZE_API_KEY);    
    
        char numOfForecastsBuff[3] = {0};
        IntegerToString(numOfForecasts,numOfForecastsBuff);
        strcat(serverPath,"http://api.openweathermap.org/data/2.5/forecast?q=");
        strcat(serverPath,prevCity);
        strcat(serverPath,",");
        strcat(serverPath,prevCountry);
        strcat(serverPath,"&cnt=");
        strcat(serverPath,numOfForecastsBuff);
        strcat(serverPath,"&appid=");
        strcat(serverPath,prevWeatherApiKey);
        
        jsonBuffer = HttpGetRequest(serverPath,&httpCode);
        JSONVar jsonObject = JSON.parse(jsonBuffer);
        uint32_t serverPathLen = strlen(serverPath);
        memset(serverPath,'\0',serverPathLen);
      
        if(httpCode == HTTP_CODE_OK)
        {
          probOfPrecip = GetPoP(jsonObject,numOfForecasts); 
          forecastReceived = true;     
          Serial.print("probability of precipitation = ");
          Serial.println(probOfPrecip);
          Serial.println("Weather forecast task suspended");
          vTaskSuspend(NULL);
        }      
        prevTime = millis();  
      }
    }
  }
}

/**
 * @brief Handles main application logic.  
*/
void ApplicationTask(void* pvParameters)
{
  const uint8_t numOfHmiConfigParams = 6;
  //Array of HMI configurable parameters
  const HMI::Config param[numOfHmiConfigParams] = 
  {
    HMI::Config::MIN_MOIST, 
    HMI::Config::MAX_MOIST,
    HMI::Config::MIN_IRRIG_TIME, 
    HMI::Config::MAX_IRRIG_TIME,
    HMI::Config::FORECAST_HOUR, 
    HMI::Config::FORECAST_MIN
  };
  //Array of data IDs for HMI configurable parameters
  const HC12::TxDataId txDataId[numOfHmiConfigParams] = 
  {
    HC12::TxDataId::MIN_MOIST,
    HC12::TxDataId::MAX_MOIST,
    HC12::TxDataId::MIN_IRRIG_TIME,
    HC12::TxDataId::MAX_IRRIG_TIME,
    HC12::TxDataId::FORECAST_HOUR,
    HC12::TxDataId::FORECAST_MIN
  };
  //Low node voltage = 10.2V [Raw value = 102]
  const uint8_t lowNodeBattLevel = 102; 
  uint8_t rowPins[NUMBER_OF_ROWS] = {18,19,23,25};  
  uint8_t columnPins[NUMBER_OF_COLUMNS] = {26,27,32,33};  
  const uint32_t smsInterval = 10000; //milliseconds
  
  static LiquidCrystal_I2C lcd(0x27,16,2);
  static Keypad keypad(rowPins,columnPins); 
  static HMI hmi(&lcd,&keypad);
  static RTC_DS3231 rtc;
  static HC12 hc12(&Serial2);
  static SIM800L gsm(&Serial1,9600,-1,5);
  
  Serial.print(rtc.begin());
  Serial.println("--> RTC status");
  hmi.RegisterClock(&rtc);
  HMI_LoadState(hmi,param,numOfHmiConfigParams,"6");
    
  lcd.init();
  lcd.backlight();
  lcd.setCursor(3,0);
  lcd.print("Auto-Irrig"); //startup message
  vTaskDelay(pdMS_TO_TICKS(1500));
  lcd.clear();  

  char prevPhoneNum[SIZE_PHONE_NUM] = {0};
  bool forecastTaskResumed = false; 
  uint32_t prevTime = millis();
  uint32_t prevBattLevelTime = millis();
  uint32_t prevWaterLevelTime = millis();
  bool isBattPrevLow = false;
  bool isTankPrevEmpty = false;
  bool isWifiTaskSuspended = false;
  
  while(1)
  {
    //Suspend WiFi Management task if the system is already.... 
    //connected to a Wi-Fi network
    if(WiFi.status() == WL_CONNECTED && !isWifiTaskSuspended)
    {
      Serial.println("WIFI TASK: SUSPENDED");
      vTaskSuspend(wifiTaskHandle);
      isWifiTaskSuspended = true;
    }
    else if(WiFi.status() != WL_CONNECTED && isWifiTaskSuspended)
    {
      Serial.println("WIFI TASK: RESUMED");
      vTaskResume(wifiTaskHandle);
      isWifiTaskSuspended = false;
    }

    hmi.Start();
    
    if(hmi.IsSaveButtonPressed())
    {
      HMI_SaveState(hmi,param,numOfHmiConfigParams,"6");
      NotifyParamSave(lcd);
    }
    
    //Resume task to obtain probability of precipitation when.... 
    //programmed forecast time is reached.
    DateTime dateTime = rtc.now();
    uint8_t currentHour = dateTime.hour();
    uint8_t currentMin = dateTime.minute();
    uint8_t currentSec = dateTime.second();
    
    if(currentHour == hmi.GetParamConfig(HMI::Config::FORECAST_HOUR) &&
       currentMin == hmi.GetParamConfig(HMI::Config::FORECAST_MIN) && 
       currentSec < 2 && !forecastTaskResumed)
    {
      vTaskResume(forecastTaskHandle);
      Serial.println("Resumed weather forecast task");
      forecastTaskResumed = true;
    }
    
    //Encode data and send to node (periodically)
    if((millis() - prevTime) >= 1200)
    { 
      hc12.EncodeData(HC12::QUERY,HC12::TxDataId::DATA_QUERY);
      for(uint8_t i = 0; i < numOfHmiConfigParams; i++)
      { 
        hc12.EncodeData(hmi.GetParamConfig(param[i]),txDataId[i]);
      }
      if(forecastReceived)
      {
        hmi.RegisterPoP(probOfPrecip);
        hc12.EncodeData(probOfPrecip,HC12::TxDataId::PROB_OF_PREC); 
        forecastTaskResumed = false; //forecast task would have suspended itself
        forecastReceived = false; //would be true when next forecast is obtained
        hmi.ForecastNotification();
      }
      hc12.EncodeData(irrigCmd,HC12::TxDataId::IRRIG_CMD); 
      hc12.EncodeData(currentHour,HC12::TxDataId::CURRENT_HOUR);
      hc12.EncodeData(currentMin,HC12::TxDataId::CURRENT_MINUTE);
      hc12.TransmitData(); 
      prevTime = millis();
    }

    //Decode data received from node
    if(hc12.ReceivedData())
    {
      if(hc12.DecodeData(HC12::RxDataId::DATA_ACK) == HC12::ACK)
      {
        Serial.println("Received data from node");
        hmi.SignalDataReception();
        soilMoisture = hc12.DecodeData(HC12::RxDataId::NODE_MOIST);
        temperature = hc12.DecodeData(HC12::RxDataId::NODE_TEMP);
        humidity = hc12.DecodeData(HC12::RxDataId::NODE_HUMID);
        //Register node's battery level in the HMI system
        uint8_t battLevel = hc12.DecodeData(HC12::RxDataId::NODE_BL);
        hmi.RegisterBattLevel(battLevel);
        //Get water level(status) and battery level(status) of node
        bool isTankEmpty = !hc12.DecodeData(HC12::RxDataId::NODE_WL);
        bool isBattLow = (battLevel <= lowNodeBattLevel);
        //Prepare to send SMS if water level and/or node's battery level is low 
        TriggerTextMsgTimer(isTankEmpty,&isTankPrevEmpty,HC12::RxDataId::NODE_WL,&prevWaterLevelTime);
        TriggerTextMsgTimer(isBattLow,&isBattPrevLow,HC12::RxDataId::NODE_BL,&prevBattLevelTime);
      }
    }
    //Send SMS when tank is empty and/or battery level is low [periodically]
    SendTextMsgForCriticalEvent(gsm,prevPhoneNum,&isTankPrevEmpty,
                                HC12::RxDataId::NODE_WL,
                                &prevWaterLevelTime,smsInterval);
                                
    SendTextMsgForCriticalEvent(gsm,prevPhoneNum,&isBattPrevLow,
                                HC12::RxDataId::NODE_BL,
                                &prevBattLevelTime,smsInterval);   
  }
}

/**
 * @brief Callback function that is called whenever WiFi
 * manager parameters are received
*/
void WiFiManagerCallback(void) 
{
  char prevPhoneNum[SIZE_PHONE_NUM] = {0};
  char prevCity[SIZE_CITY_NAME] = {0};
  char prevCountry[SIZE_COUNTRY_CODE] = {0};
  char prevWeatherApiKey[SIZE_API_KEY] = {0}; 
  char prevPubTopic[SIZE_TOPIC] = {0};
  char prevSubTopic[SIZE_TOPIC] = {0};
  char prevClientID[SIZE_CLIENT_ID] = {0};
  
  preferences.getBytes("0",prevPhoneNum,SIZE_PHONE_NUM);
  preferences.getBytes("1",prevCity,SIZE_CITY_NAME);
  preferences.getBytes("2",prevCountry,SIZE_COUNTRY_CODE);  
  preferences.getBytes("3",prevWeatherApiKey,SIZE_API_KEY);
  preferences.getBytes("4",prevPubTopic,SIZE_TOPIC);  
  preferences.getBytes("5",prevSubTopic,SIZE_TOPIC);  
  preferences.getBytes("A",prevClientID,SIZE_CLIENT_ID);

  StoreNewFlashData("0",phoneNum.getValue(),prevPhoneNum,SIZE_PHONE_NUM);
  StoreNewFlashData("1",city.getValue(),prevCity,SIZE_CITY_NAME);
  StoreNewFlashData("2",country.getValue(),prevCountry,SIZE_COUNTRY_CODE);
  StoreNewFlashData("3",weatherApiKey.getValue(),prevWeatherApiKey,SIZE_API_KEY);  
  StoreNewFlashData("4",pubTopic.getValue(),prevPubTopic,SIZE_TOPIC);
  StoreNewFlashData("5",subTopic.getValue(),prevSubTopic,SIZE_TOPIC);
  StoreNewFlashData("A",clientID.getValue(),prevClientID,SIZE_CLIENT_ID);
}

/**
 * @brief Callback function that is called whenever data is received
 * from the HiveMQ broker.  
*/
void MqttCallback(char *topic,byte *payload,uint32_t len) 
{
  Serial.print("MQTT receive:");
  irrigCmd = (payload[0] - '0');
  Serial.println(irrigCmd);
}
