# Automated_Irrigation_System

## Description  
An Automated Irrigation System designed to optimize irrigation processes in the farm.    

## Features   
1. Irrigation control based on:   
  - soil moisture 
  - amount of water in a tank  
  - weather forecast (probability of precipitation)  
2. Remote monitoring and control of the system.    
3. SMS alerts to be triggered when the tank's water level is low or when the battery level of the node is low. 
4. Wi-Fi provisioning.  

NB: Evening irrigation occurs at 6pm and it depends on soil moisture and amount of water in the tank.   

## Architecture  
The system is split into 3 parts. These are:  
1. The master device,    
2. The node(s), and  
3. The water level detector.  
In order to reduce cost of prototyping/development, the node and water level detector are merged into a single unit.  

The master consists of the following:  
1. An HMI     
2. WiFi   
3. GSM   
4. Sub-GHz radio system (low power, long range)  
5. A microcontroller.  

The node + water level detector unit consists of:  
1. A moisture sensor  
2. A temperature & humidity sensor    
3. Sub-GHz radio system  
4. Solenoid valve  
5. Water hose  
6. Ultrasonic sensor  
7. A microcontroller.  

The master performs the following:  
1. Sends sensor readings (taken by the node) to the cloud via MQTT.  
2. Communicates with the node (via Sub-GHz radio). It receives sensor readings from the node,  
as well as an alert from the water level detector (during critical conditions).     
3. Sends SMS alerts to the user's mobile phone whenever water level is low.    
4. Retrieves weather forecast online in order to aid irrigation decisions.  
5. Configures sensor thresholds within which the node determines when to irrigate.   

The node performs the following:  
1. Obtains sensor readings (moisture and temperature) and sends to the master   
wirelessly (via Sub-GHz radio).  
2. Controls irrigation via sensor readings and thresholds programmed into the master.  
These thresholds are sent wirelessly from master to node.  

The water level detector simply checks the level of water in the tank. As explained earlier,  
it is merged with the node. If there's a critical condition, a message is sent to the master wirelessly  
(via Sub-GHz radio). The master will then send an SMS to the user.  

## Software architecture  
![ss_sl drawio](https://user-images.githubusercontent.com/46250887/224769995-1d8432c9-96d5-4adb-b228-826eac6d4588.png)  

## Mobile Application  
The sensor data from the irrigation system is sent to an MQTT broker. The ``HiveMQ`` broker was used in this  
project. The user can use a mobile application (MQTT client) that is linked to the broker to visualize the  
sensor data. The ``MQTT Dashboard`` app from playstore was used. Details of the broker used are given below:  

- Broker name: HiveMQ  
- Address: tcp://broker.hivemq.com  
- Port: 1883  

The application (MQTT client) is configured as follows:  
- Subscription topic: Same as the topic configured during Wi-Fi provisioning
- Publish topic: Same as the topic configured during Wi-Fi provisioning
- Start button: To enable irrigation. It publishes data (i.e. a ``payload`` of `` `1` `` to the ``Publish topic``)  
- Stop buttton: To disable irrigation. It publishes data (i.e. a ``payload`` of `` `1` `` to the ``Publish topic``)
- Text box: To display data. It subscribes to the ``Subscription topic``.   

## Tasks  
1. Getting weather forecast from ``OpenWeatherMap.org`` with an ``ESP32`` [DONE]        
2. Testing ``moisture sensor`` [DONE]    
3. Setting up an ``RTOS`` application for the ``Master``.  [DONE]  
4. Setting up ``WiFi provisioning`` for the ``ESP32`` using the following details: [DONE]  
- SSID  
- Password  
- City name + country code + API key to obtain weather forecast, and more.  
5. Testing the ``SIM800L`` library [DONE]  
6. LCD test [DONE]  
7. Keypad test  [DONE]  
8. HMI [DONE]   
9. MQTT [DONE]  
10. Irrigation decision making [DONE]   
11. Battery monitoring for the node [DONE]    

## Images of the prototypes
![20230408_114212](https://user-images.githubusercontent.com/46250887/230763241-ff751680-f3bf-472a-a32d-2395808fdcdb.jpg)  
![20230408_114132](https://user-images.githubusercontent.com/46250887/230763270-3a0bf62c-e23b-472f-9812-4653e6b8b9cd.jpg)   
![20230214_100111](https://user-images.githubusercontent.com/46250887/218693879-73e67614-5af9-423c-8325-8bc9774a5427.jpg)
![20230214_100123](https://user-images.githubusercontent.com/46250887/218694133-0fc30c0f-23cc-47db-8af6-e70f6a2a6c4d.jpg)
![20230214_100029](https://user-images.githubusercontent.com/46250887/218694005-0c281f36-b9c9-49fc-a606-627eb4c2ca4c.jpg)
![20230208_085441](https://user-images.githubusercontent.com/46250887/217487152-40a2b927-eecf-4694-b2be-4e5b8109bda4.jpg)  
![20230208_085742](https://user-images.githubusercontent.com/46250887/217489647-36b613c2-e0d3-4223-aa7e-426ae2b9ebce.jpg)  
![MQTT dashboard](https://user-images.githubusercontent.com/46250887/222798335-11319a68-478c-46b1-a83a-e7d2179cc261.jpg)  
![20230408_164155](https://user-images.githubusercontent.com/46250887/230763356-eeeff724-2479-4ee6-bb76-c9c0a20ad5fb.jpg)  

## Observation  
- The GND pin beside the 5v pin on some ESP32 boards isn't connected to the other GND pins because its supposed  
to be the CMD pin. Hence this pin shouldn't be connected to the GND pin of other components without first performing   
continuity tests with a multimeter.  
- The Arduino Nano ADC doesn't work properly if the board is powered using Vin. This issue was solved by powering the board  
through the 5v pin.  

## Credits  
1. OpenWeatherMap.org: https://RandomNerdTutorials.com/esp32-http-get-open-weather-map-thingspeak-arduino/   
2. Firebase ESP32 Client: https://randomnerdtutorials.com/esp32-firebase-realtime-database/  [NOW DEPRECATED, MQTT is desired]  
3. MQTT with ESP32: https://microcontrollerslab.com/esp32-mqtt-client-publish-subscribe-bme280-readings-hivemq/    
4. OpenWeatherMap.org API calls (3-hour forecast): https://openweathermap.org/forecast5  
