#pragma once

class SIM800L
{
  private:
    HardwareSerial* port;
  public:
    SIM800L(HardwareSerial* serial,
            uint32_t baudRate = 9600,
            int8_t simTx = -1,
            int8_t simRx = -1);
    void SendSMS(char* phoneNum,char* msg);
};

