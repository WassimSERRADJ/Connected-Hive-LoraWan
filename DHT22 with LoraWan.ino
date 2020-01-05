#include <Arduino.h>
#include "TheThingsNetwork.h"
#include "CayenneLPP.h"
#include <Adafruit_Sensor.h>
// Board Definitions
#define bleSerial Serial1
#define loraSerial Serial2
#define debugSerial SerialUSB
#define SERIAL_TIMEOUT  10000

#define ADC_AREF 3.3f
#define BATVOLT_R1 4.7f
#define BATVOLT_R2 10.0f
#define BATVOLT_PIN BAT_VOLT

#include <DHT.h>
#include <DHT_U.h>

#define debugSerial SerialUSB
#define DHTPIN 11
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);                                                
// LoRa Definitions and constants
// For OTAA. Set your AppEUI and AppKey. DevEUI will be serialized by using  HwEUI (in the RN module)
const char *appEui = "";
const char *appKey = "";

const bool CNF   = true;
const bool UNCNF = false;
const byte MyPort = 3;
byte Payload[51];
byte CNT = 0;                                               // Counter for the main loop, to track packets while prototyping
#define freqPlan TTN_FP_EU868                              // Replace with TTN_FP_EU868 or TTN_FP_US915
#define FSB 0                                               // FSB 0 = enable all channels, 1-8 for private networks
#define SF 7                                                // Initial SF


/*
// For ABP. Set your devAddr & Keys
const char *devAddr = "";
const char *nwkSKey = "";
const char *appSKey = "";
*/
enum state {WHITE, RED, GREEN, BLUE, CYAN, ORANGE, PURPLE, OFF};    // List of colours for the RGB LED
byte BUTTON_STATE;  // Tracks the previous status of the button
TheThingsNetwork  ExpLoRer (loraSerial, debugSerial, freqPlan, SF, FSB);    // Create an instance from TheThingsNetwork class
CayenneLPP        CayenneRecord (51);                                       // Create an instance of the Cayenne Low Power Payload


void setup()
{ 
  pinMode(BUTTON, INPUT_PULLUP);
  BUTTON_STATE = !digitalRead(BUTTON);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  LED(RED);                             // Start with RED
  pinMode(TEMP_SENSOR, INPUT);
  analogReadResolution(10);             //Set ADC resolution to 10 bits
  
  pinMode(LORA_RESET, OUTPUT);          //Reset the LoRa module to a known state
  digitalWrite(LORA_RESET, LOW);
  delay(100);
  digitalWrite(LORA_RESET, HIGH);
  delay(1000);                          // Wait for RN2483 to reset
  LED(ORANGE);                          // Switch to ORANGE after reset

  
  loraSerial.begin(57600);
  debugSerial.begin(57600);
  // Wait a maximum of 10s for Serial Monitor
  while (!debugSerial && millis() < SERIAL_TIMEOUT);

  // Set callback for incoming messages
  ExpLoRer.onMessage(message);

  //Set up LoRa communications
  debugSerial.println("-- STATUS");
  ExpLoRer.showStatus();

  delay(1000);

  debugSerial.println("-- JOIN");

  // For OTAA. Use the 'join' function 
  if (ExpLoRer.join(appEui, appKey, 6, 5000))   // 6 Re-tries, 3000ms delay in between
    LED(GREEN);               // Switch to GREEN if OTAA join is successful
  else
    LED(RED);                 // Switch to RED if OTAA join fails

  // For ABP. Use the 'personalize' function instead of 'join' above
  //ExpLoRer.personalize(devAddr, nwkSKey, appSKey);

  delay(1000);

  //Hard-coded GPS position, just to create a map display on Cayenne
  CayenneRecord.reset();
  CayenneRecord.addGPS(1, 51.4141391, -0.9412872, 10);  // Thames Valley Science Park, Shinfield, UK               
  // Copy out the formatted record
  byte PayloadSize = CayenneRecord.copy(Payload);
  dht.begin();
  ExpLoRer.sendBytes(Payload, PayloadSize, MyPort, UNCNF);
  CayenneRecord.reset();
}                                                       // End of the setup() function


void loop()
{
  CNT++;
  debugSerial.println(CNT, DEC);

  float MyTemp = getTemperature();                    // Onboard temperature sensor
  CayenneRecord.addTemperature(3, MyTemp);
  float RelativeHumidity = getHumidity();                         // Grove sound sensor on pin A8
  CayenneRecord.addRelativeHumidity(4,RelativeHumidity);
   CayenneRecord.addAnalogInput(2, getBatteryVoltage());
  SerialUSB.println(getBatteryVoltage());
  // When all measurements are done and the complete Cayenne record created, send it off via LoRa
  LED(BLUE);                                          // LED on while transmitting. Green for energy-efficient LoRa
  byte PayloadSize = CayenneRecord.copy(Payload);
  byte response = ExpLoRer.sendBytes(Payload, PayloadSize, MyPort, UNCNF);

  LED(response +1);                                   // Change LED colour depending on module response to uplink success
  delay(100);
  
  CayenneRecord.reset();  // Clear the record buffer for the next loop
  LED(OFF);

  // Delay to limit the number of transmissions
  for (byte i=0; i<8; i++)    // 8+2second delay. Additional ~2sec delay during Tx
    {
      // LED(i % 8);          // Light show, to attract attention at tradeshows!
      delay(500); 
      LED(OFF);
      delay(500);
      
    }
}                                                       // End of loop() function


// Helper functions below here                                                

float getTemperature()
{
// La lecture du capteur prend 250ms
 // Les valeurs lues peuvet etre vieilles de jusqu'a 2 secondes (le capteur est lent)
 float t = dht.readTemperature();//on lit la temperature en celsius (par defaut)
 // pour lire en farenheit, il faut le paramère (isFahrenheit = true) :
 delay(2000);

 //On verifie si la lecture a echoue, si oui on quitte la boucle pour recommencer.
 if (isnan(t))
 {
   debugSerial.println("Failed to read from DHT sensor!");
   return getTemperature();
 }
  
  return t;
}
float getHumidity()
{
// La lecture du capteur prend 250ms
 // Les valeurs lues peuvet etre vieilles de jusqu'a 2 secondes (le capteur est lent)
 float h = dht.readHumidity();//on lit l'hygrometrie
 delay(2000);

 //On verifie si la lecture a echoue, si oui on quitte la boucle pour recommencer.
 if (isnan(h))
 {
   debugSerial.println("Failed to read from DHT sensor!");
   return getHumidity();
 }
  
  return h;
}


void LED(byte state)
{
  switch (state)
  {
  case WHITE:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW); 
    break;
  case RED:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH); 
    break;
  case ORANGE:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH); 
    break;
  case CYAN:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW); 
    break;
  case PURPLE:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    break;
  case BLUE:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    break;
  case GREEN:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    break;
  default:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    break;
  }
}

// Callback method for receiving downlink messages. Uses ExpLoRer.onMessage(message) in setup()
void message(const uint8_t *payload, size_t size, port_t port)
{
  debugSerial.println("-- MESSAGE");
  debugSerial.print("Received " + String(size) + " bytes on port " + String(port) + ":");

  for (int i = 0; i < size; i++)
  {
    debugSerial.print(" " + String(payload[i]));
  }

  debugSerial.println();
}
uint16_t getBatteryVoltage()
{
    uint16_t voltage = (uint16_t)((ADC_AREF / 1.023) * (BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2 * (float)analogRead(BATVOLT_PIN));
    voltage = voltage / 4024 * 100;
    return voltage;
}
