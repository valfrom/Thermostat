#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>   // Adafruit library for Nokia 5110 LCD display
#include <OneWire.h>
#include <EEPROM.h>
#include <Manchester.h>

// pin 8 - Serial clock out (SCLK)
// pin 7 - Serial data out (DIN)
// pin 6 - Data/Command select (D/C)
// pin 5 - LCD chip select (CS)
// pin 4 - LCD reset (RST)
//Adafruit_PCD8544 nokia = Adafruit_PCD8544(8, 7, 6, 5, 4);  // create the LCD control device
//Adafruit_PCD8544 nokia = Adafruit_PCD8544(3, 4, 5, 6, 7);  // create the LCD control device
Adafruit_PCD8544 nokia = Adafruit_PCD8544(3, 4, 5, 6, 7);  // create the LCD control device

#define CURSOR(x,y)  nokia.setCursor(x,y)  // we end up doing this a lot
#define CONTRAST_VALUE 55

volatile unsigned long time = 0;
volatile unsigned long previousTime = 0;
volatile unsigned long time2 = 0;

volatile unsigned long duration = 0;
volatile unsigned long startTime;

volatile unsigned int tick = 0;

volatile int state = 0;

int lowTemp = 0;
int highTemp = 0;
int dstTemp = 1900;
int gest = 6;
int oldC = 0;
int oldDstTemp = 0;

int backlitOn = true;

int humidity = 0;

int turnedOn = 0;

int counter = 0;

volatile int currentTick = 0;

char buffer[255] = {0};

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 8

#define SENDER_ID 0

#define BACKLIT_PIN 2

#define CENTER_BUTTON 10
#define DOWN_BUTTON A2
#define UP_BUTTON A3

#define LOAD_PIN 11
#define TX_PIN A1

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);


void checkTemperature(int temp);

//This function will write a 2 byte integer to the eeprom at the specified address and address + 1
void EEPROMWriteInt(int p_address, int p_value) {
  byte lowByte = ((p_value >> 0) & 0xFF);
  byte highByte = ((p_value >> 8) & 0xFF);
    
  EEPROM.write(p_address, lowByte);
  EEPROM.write(p_address + 1, highByte);
}

//This function will read a 2 byte integer from the eeprom at the specified address and address + 1
unsigned int EEPROMReadInt(int p_address) {
  byte lowByte = EEPROM.read(p_address);
  byte highByte = EEPROM.read(p_address + 1);
  
  return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}

void setTemp(int newTemp) {
  dstTemp = newTemp;
  lowTemp = dstTemp - 20;
  highTemp = dstTemp + 20;
  EEPROMWriteInt(0, dstTemp);
}

void turnOn() {
  digitalWrite(LOAD_PIN, HIGH);
  for (int i=0;i<10;i++) {
    man.transmit(man.encodeMessage(SENDER_ID, 1));
  }
  turnedOn = true;
}

void turnOff() {
  digitalWrite(LOAD_PIN, LOW);
  for (int i=0;i<30;i++) {
    man.transmit(man.encodeMessage(SENDER_ID, 0));
  }
  turnedOn = false;
}

void setBacklitOn(int state) {
  if(state) {
    digitalWrite(BACKLIT_PIN, HIGH);
  } else {
    digitalWrite(BACKLIT_PIN, LOW);
  }
  backlitOn = state;
  EEPROMWriteInt(2, state);
}

void setup() {
  delay(1000);
  
  int t = EEPROMReadInt(0);
  
  if(t < 10) {
    t = 1900;
  }
  
  setTemp(t);
  
  int b = EEPROMReadInt(2);
  setBacklitOn(b);
  
  man.workAround1MhzTinyCore(); //add this in order for transmitter to work with 1Mhz Attiny85/84
  man.setupTransmit(TX_PIN, MAN_300);
  
  sensors.setResolution(TEMP_12_BIT);
  sensors.begin();

  pinMode(CENTER_BUTTON, INPUT_PULLUP);
  pinMode(DOWN_BUTTON, INPUT_PULLUP);
  pinMode(UP_BUTTON, INPUT_PULLUP);

  pinMode(LOAD_PIN, OUTPUT);
  digitalWrite(LOAD_PIN, LOW);
  
  nokia.begin(CONTRAST_VALUE);
  nokia.fillScreen(1);  
  nokia.setTextSize(2);
  nokia.display();
  
  turnOff();
  pinMode(BACKLIT_PIN, OUTPUT);
  digitalWrite(BACKLIT_PIN, HIGH);
}

void up() {
  setTemp(dstTemp + 10);
}

void down() {
  setTemp(dstTemp - 10);
}

void center() {
  setBacklitOn(!backlitOn);
}

void loop() {

  sensors.requestTemperatures();
  
  CURSOR(0,0);
  nokia.fillScreen(0);
  
  int c = sensors.getTempCByIndex(0) * 100;
  
  if(c < 0) {
    nokia.print("ERROR!\nTEMP\nSENSOR!");
    nokia.display();
    turnOff();
    return;
  }
  
  checkTemperature(c);
  
  int c1 = c / 100;  
  
  int c2 = (c % 100) / 10;
  
  int dest = (highTemp + lowTemp) / 2;
  
  int d1 = dest / 100;
  int d2 = (dest % 100) / 10;
  
  char *heat = "";
  
  if(turnedOn) {
    heat = "HEAT";
  }
  
  sprintf(buffer, "T%d.%dC\n%s\nD%d.%dC", c1, c2, heat, d1, d2);

  nokia.print(buffer);
  nokia.display();
  
  delay(10);
  
  if(digitalRead(CENTER_BUTTON) == 0) {
    center();
  }
  if(digitalRead(DOWN_BUTTON) == 0) {
    down();
  }
  if(digitalRead(UP_BUTTON) == 0) {
    up();
  }
  
  counter ++;
  if(counter % 1000 == 0) {
    if(turnedOn) {
      turnOn();
    } else {
      turnOff();
    }
  }
}


void checkTemperature(int temp) {
  if(turnedOn && temp >= highTemp) {
    currentTick = 0;
    turnOff();
    return;
  }
  if((++currentTick) % gest != 0) {
    return;
  }
  if(!turnedOn && temp <= lowTemp) {
    currentTick = 0;
    turnOn();
  }
}
