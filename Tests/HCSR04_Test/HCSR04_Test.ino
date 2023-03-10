//HCSR04 pins
const uint8_t trigPin = 9;
const uint8_t echoPin = 10;

const uint8_t lowWaterLevel = 10; // >=
const uint8_t highWaterLevel = 5; // <= 

static uint32_t prevTime;

static uint16_t GetDistance(void)
{
  //Sensor trigger
  digitalWrite(trigPin,HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin,LOW);
  //Obtaining distance in cm
  uint32_t pulseWidth = pulseIn(echoPin,HIGH);
  uint16_t distance = lround(pulseWidth / 58.0);
  return distance;
}

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
  prevTime = millis();
}

void loop() 
{
  // put your main code here, to run repeatedly:
  if((millis() - prevTime) >= 2000)
  {
    Serial.print("distance = ");
    Serial.println(GetDistance());
    prevTime = millis();
  }
  
}
