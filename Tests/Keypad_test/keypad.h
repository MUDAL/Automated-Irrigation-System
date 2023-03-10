#pragma once

#define NUMBER_OF_ROWS      4
#define NUMBER_OF_COLUMNS   4

class Keypad
{
  private:
    uint8_t* pRow;
    uint8_t* pCol;
    bool pinPrevPressed[NUMBER_OF_ROWS][NUMBER_OF_COLUMNS];
    void SelectRow(uint8_t pinIndex);
    bool IsDebounced(uint8_t pinIndex);
    
  public:
    Keypad(uint8_t* pRowPins,uint8_t* pColPins);
    char GetChar(void);
};

