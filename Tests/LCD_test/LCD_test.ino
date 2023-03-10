#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

void setup() 
{
  // put your setup code here, to run once:
  lcd.init();
  lcd.backlight();
  lcd.setCursor(2,1);
  lcd.print("Hello World!");
}

void loop() 
{
  // put your main code here, to run repeatedly:

}
