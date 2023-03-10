#include "keypad.h"

uint8_t rowPins[NUMBER_OF_ROWS] = {18,19,23,25};  
uint8_t columnPins[NUMBER_OF_COLUMNS] = {26,27,32,33};
Keypad keypad(rowPins,columnPins); 

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);
}

void loop() 
{
  // put your main code here, to run repeatedly:
  char key = keypad.GetChar();
  if(key != '\0')
  {
    Serial.print("Key = ");
    Serial.println(key);
  }
}
