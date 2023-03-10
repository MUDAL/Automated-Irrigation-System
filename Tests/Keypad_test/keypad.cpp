#include <Arduino.h>
#include "keypad.h"

const char keypadMatrix[NUMBER_OF_ROWS][NUMBER_OF_COLUMNS] =
{{'1','2','3','A'},
 {'4','5','6','B'},
 {'7','8','9','C'},
 {'*','0','#','D'}};

/*
 * @brief Selects a row of the keypad whose columns will be read
 * @param pinIndex: Index of the row to be selected.
 * @example The first row has an index of 0, the second has an index of 1 etc.
 * @return None
*/
void Keypad::SelectRow(uint8_t pinIndex)
{
  for(uint8_t i = 0; i < NUMBER_OF_ROWS; i++)
  {
    if(i == pinIndex)
    {
      digitalWrite(pRow[i],LOW);
    }
    else
    {
      digitalWrite(pRow[i],HIGH);
    }
  }
}

/*
 * @brief Checks if a key(column) on the keypad is free from bouncing
 * @param pinIndex: Index of the column/input 
 * @example The first column has an index of 0, the second has an index of 1 etc.
 * @return True if the column has been debounced and false if otherwise
*/
bool Keypad::IsDebounced(uint8_t pinIndex)
{
  if(digitalRead(pCol[pinIndex]) == LOW)
  {
    vTaskDelay(pdMS_TO_TICKS(15)); //De-bounce delay
    if(digitalRead(pCol[pinIndex]) == LOW)
    {
      return true;
    }
  }
  return false;
}

/*
 * @brief Initializes an object of the Keypad class
 * @param pRowPins: starting address of the array of pins for the keypad's rows
 * @param pColPins: starting address of the array of pins for the keypad's columns
 * @return None
*/
Keypad::Keypad(uint8_t* pRowPins,uint8_t* pColPins)
{
  //Initialize private variables
  pRow = pRowPins;
  pCol = pColPins;
  for(uint8_t i = 0; i < NUMBER_OF_ROWS; i++)
  {
    for(uint8_t j = 0; j < NUMBER_OF_COLUMNS; j++)
    {
      pinPrevPressed[i][j] = false;
    }
  }  
  //Initialize rows
  for(uint8_t i = 0; i < NUMBER_OF_ROWS; i++)
  {
    pinMode(pRowPins[i],OUTPUT);
  }
  //Initialize columns
  for(uint8_t i = 0; i < NUMBER_OF_COLUMNS; i++)
  {
    pinMode(pColPins[i],INPUT_PULLUP);
  }
}

/*
 * @brief Gets the character corresponding to the pressed key on the keypad
 * @param None
 * @return character e.g. '*' if * is pressed, 'A' if A is pressed, etc.
*/
char Keypad::GetChar(void)
{
  for(uint8_t i = 0; i < NUMBER_OF_ROWS; i++)
  {
    Keypad::SelectRow(i);
    for(uint8_t j = 0; j < NUMBER_OF_COLUMNS; j++)
    {
      if(Keypad::IsDebounced(j) && !pinPrevPressed[i][j])
      {
        pinPrevPressed[i][j] = true;
        return keypadMatrix[i][j];
      }
      else if(!Keypad::IsDebounced(j) && pinPrevPressed[i][j])
      {
        pinPrevPressed[i][j] = false;
      }
    }
  }
  return '\0';
}
