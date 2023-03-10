#pragma once
 
class HC12
{
  private:
    enum BufferSize {TX = 6, RX = 11};
    SoftwareSerial* port;
    uint8_t rxDataCounter;
    uint8_t txBuffer[BufferSize::TX]; 
    uint8_t rxBuffer[BufferSize::RX];
    
  public:
    enum {QUERY = 0xAA, ACK = 0xBB};
    enum TxDataId
    {
      DATA_ACK = 0,
      NODE_MOIST,
      NODE_TEMP,
      NODE_HUMID,
      NODE_WL, //water level
      NODE_BL  //battery level
    };
    enum RxDataId 
    {
      DATA_QUERY = 0,
      MIN_MOIST,
      MAX_MOIST,
      MIN_IRRIG_TIME,
      MAX_IRRIG_TIME,
      CURRENT_HOUR,
      CURRENT_MINUTE,
      FORECAST_HOUR,
      FORECAST_MIN,
      PROB_OF_PREC,
      IRRIG_CMD //remote irrigation command
    };
    
    HC12(SoftwareSerial* serial,uint32_t baudRate = 9600);
    /*Transmitter*/
    void EncodeData(uint8_t dataToEncode,TxDataId id);
    void TransmitData(void);
    /*Non-blocking Receiver*/
    bool ReceivedData(void);
    uint8_t DecodeData(RxDataId id);
};
