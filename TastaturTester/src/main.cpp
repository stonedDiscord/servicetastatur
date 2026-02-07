/*

Sending data through 2 shift registers 74HC4094

The Data first goes into U20 which is at address 0x0040 and then into U19 which is at address 0x0050
Both registers have all the data lines reversed and U19 has D1 and D3 swapped.
 */

#include <Arduino.h>

 // Pin connected to STR of 74HC4094
int strobePin = 8;
// Pin connected to CP of 74HC4094
int clockPin = 12;
// Pin connected to D of 74HC4094
int dataOutPin = 11;

int dataInPin = 7;

unsigned int dataIn = 0;

unsigned int initArray[8] = {
    0xEFF7, 0xBFDF, 0xBFDF, 0xBFDF, 0xEFF7, 0xEFF7, 0xBFDF, 0xBFDF,
};

byte protectionSwap(byte value)
{
  byte d1 = (value >> 1) & 1;
  byte d3 = (value >> 3) & 1;
  value &= ~(1 << 1);
  value &= ~(1 << 3);
  value |= (d3 << 1);
  value |= (d1 << 3);
  return value;
}

byte reverseAll(byte indata)
{
  byte out = 0;
  for (int i = 0; i < 8; i++) {
    out |= ((indata >> i) & 1) << (7 - i);
  }
  return out;
}

void shiftOut16(int dataOutPin, int clockPin, unsigned int rawValue)
{
  byte lowByte = (rawValue >> 8) & 0xFF;
  byte highByte = rawValue & 0xFF;
  byte processedHigh = reverseAll(highByte);
  byte processedLow = reverseAll(lowByte); //protectionSwap(reverseAll(lowByte));
  unsigned int value = (processedHigh << 8) | processedLow;

  digitalWrite(strobePin, LOW);

  for (int i = 15; i >= 0; i--)
  {

    digitalWrite(clockPin, LOW);

    digitalWrite(dataOutPin, (value >> i) & 1);
    dataIn = (dataIn << 1) | digitalRead(dataInPin);
    digitalWrite(clockPin, HIGH);
    delay(1);
  }

  digitalWrite(clockPin, LOW);
  delay(1);
  digitalWrite(strobePin, HIGH); // latch outputs
  delay(1);
  digitalWrite(strobePin, LOW);
}

void printChar(byte charCode)
{
  byte highNib = (charCode >> 4) & 0xF;
  byte lowNib = charCode & 0xF;
  byte highByte = (highNib << 4) | 0x3;
  byte lowByte = (0x5 << 4) | lowNib;
  unsigned int value = (highByte << 8) | lowByte;
  shiftOut16(dataOutPin, clockPin, value);
}

void setup()
{

  // set pins to output because they are addressed in the main loop
  pinMode(strobePin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataOutPin, OUTPUT);
  pinMode(dataInPin, INPUT);

  // Initialize strobe low to enable shifting
  digitalWrite(strobePin, LOW);

  Serial.begin(9600);

  for (int i = 0; i < 8; i++) {
    shiftOut16(dataOutPin, clockPin, initArray[i]);
  }
}



void loop()
{
  for (int i = 0; i < 8; i++) {
    shiftOut16(dataOutPin, clockPin, initArray[i]);
    Serial.println(dataIn, HEX);
    delay(100);
  }
  
}
