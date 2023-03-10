#include <Arduino.h>
#include <string.h>
#include "sim800l.h"

/**
 * @brief Initialize SIM800L module.  
*/
SIM800L::SIM800L(HardwareSerial* serial,
                 uint32_t baudRate,
                 int8_t simTx,
                 int8_t simRx)
{
  //Initialize private variables
  port = serial;
  port->begin(baudRate,SERIAL_8N1,simTx,simRx);
}

/**
 * @brief Send SMS to specified phone number.
*/
void SIM800L::SendSMS(char* phoneNum,char* msg)
{
  const uint8_t endOfMsgCmd = 26;
  char atCmgsCmd[27] = "AT+CMGS=\"";
  port->write("AT+CMGF=1\r\n");
  vTaskDelay(pdMS_TO_TICKS(250));
  strcat(atCmgsCmd,phoneNum);
  strcat(atCmgsCmd,"\"\r\n");
  port->write(atCmgsCmd);
  vTaskDelay(pdMS_TO_TICKS(250));
  port->write(msg);
  port->write("\r\n");
  vTaskDelay(pdMS_TO_TICKS(250));
  port->write(endOfMsgCmd); //command termination
  vTaskDelay(pdMS_TO_TICKS(250));  
}

