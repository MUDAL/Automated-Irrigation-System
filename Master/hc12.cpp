#include <Arduino.h>
#include "hc12.h"

/**
 * @brief Initialize HC12 transceiver.  
*/
HC12::HC12(HardwareSerial* serial,
           uint32_t baudRate,
           int8_t hc12Tx,
           int8_t hc12Rx)
{
  //Initialize private variables
  port = serial;
  port->begin(baudRate,SERIAL_8N1,hc12Tx,hc12Rx);
  rxDataCounter = 0;
  for(uint8_t i = 0; i < BufferSize::TX; i++)
  {
    txBuffer[i] = 0;
  }
  for(uint8_t i = 0; i < BufferSize::RX; i++)
  {
    rxBuffer[i] = 0;
  }  
}

/**
 * @brief Encodes data into the 'txBuffer' before 
 * transmission to an HC12 receiver can occur.  
 * @param dataToEncode:   data to be encoded  
 * @param id: location of specific data within 'txBuffer' to be encoded.
*/
void HC12::EncodeData(uint8_t dataToEncode,TxDataId id)
{
  uint8_t dataID = (uint8_t)id;
  txBuffer[dataID] = dataToEncode;
}

/**
 * @brief Sends data to an HC12 receiver.  
*/
void HC12::TransmitData(void)
{
  Serial.print("Tx buffer: ");
  for(uint8_t i = 0; i < BufferSize::TX; i++)
  {
    port->write(txBuffer[i]);
    //Debug 
    Serial.print(txBuffer[i]);
    Serial.print(' ');
  }
  Serial.print("\n");
}

/**
 * @brief  Receives data from an HC12 transmitter. 
 * @return true:  if data reception is complete,
 *         false: if otherwise.
*/
bool HC12::ReceivedData(void)
{
  bool rxDone = false;
  if(port->available())
  {
    if(rxDataCounter < BufferSize::RX)
    {
      rxBuffer[rxDataCounter] = port->read();
      rxDataCounter++; 
    }
  }
  if(rxDataCounter == BufferSize::RX)
  {
    rxDataCounter = 0;
    rxDone = true;
  }
  return rxDone;
}

/**
 * @brief Decodes data received from HC12 transmitter.  
 * @param id: location of specific data within 'rxBuffer' to be decoded.  
 * @return decoded data (or actual value of desired parameter).
*/
uint8_t HC12::DecodeData(RxDataId id)
{
  uint8_t dataID = (uint8_t)id;
  return rxBuffer[dataID]; 
}
