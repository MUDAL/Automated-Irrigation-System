#pragma once   

class HMI
{
  private:
    //Types
    enum State 
    {
      ST_MOISTURE, 
      ST_IRRIG_TIME, 
      ST_FORECAST, 
      ST_POP, 
      ST_BATT_LEVEL,
      ST_TIME
    };
    //Objects
    LiquidCrystal_I2C* lcdPtr;
    Keypad* keypadPtr;
    RTC_DS3231* rtcPtr;
    
    State currentState;
    uint8_t currentRow; //LCD
    bool saveButtonPressed;
    uint8_t probOfPrecip;
    uint8_t battLevel;
    bool signalFlag;
    //Parameters to configure on the device
    uint8_t minMoist;
    uint8_t maxMoist;
    uint8_t minIrrigTime;
    uint8_t maxIrrigTime;
    uint8_t forecastHour;
    uint8_t forecastMinute;
    //Parameter buffers
    char minMoistBuff[3];
    char maxMoistBuff[3];
    char minIrrigBuff[3];
    char maxIrrigBuff[3];
    char forecastHrBuff[3];
    char forecastMinBuff[3];
    
    void AlignData(uint8_t param);    
    //Keypad Input methods
    void StoreKey(char key,char* keyBuffer,uint8_t len);
    void SetParam(uint8_t* paramPtr,char* paramBuffer,uint8_t paramColumn);
    //Display methods
    void PointToRow(char* heading1,char* heading2,uint8_t row);
    //State methods
    void ChangeStateTo(State nextState);
    void StateFunc_Moisture(void);
    void StateFunc_IrrigTime(void);
    void StateFunc_Forecast(void);
    void StateFunc_PoP(void);
    void StateFunc_BattLevel(void);
    void StateFunc_DisplayTime(void);
  public:
    enum Config {MIN_MOIST, MAX_MOIST,
                 MIN_IRRIG_TIME, MAX_IRRIG_TIME,
                 FORECAST_HOUR, FORECAST_MIN};
        
    HMI(LiquidCrystal_I2C* lcdPtr,Keypad* keypadPtr);
    void RegisterClock(RTC_DS3231* rtcPtr);
    void Start(void);
    uint8_t GetParamConfig(Config paramConfig);
    void SetParamConfig(Config paramConfig,uint8_t setting);
    bool IsSaveButtonPressed(void);
    void RegisterPoP(uint8_t probOfPrecip);
    void RegisterBattLevel(uint8_t battLevel);
    void SignalDataReception(void);
    void ForecastNotification(void);
};

