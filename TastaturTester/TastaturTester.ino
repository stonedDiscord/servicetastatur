/*

Sending data through 2 shift registers 74HC4094

The Data first goes into U20 which is at address 0x0040 and then into U19 which is at address 0x0050
Both registers have all the data lines reversed and U19 has D1 and D3 swapped.
 */
// Pin connected to STR of 74HC4094
int strobePin = 8;
// Pin connected to CP of 74HC4094
int clockPin = 12;
// Pin connected to D of 74HC4094
int dataPin = 11;

unsigned int dataArray[6] = {
    /*TODO*/
};

byte protectionSwap(byte value)
{
  /*TODO*/
  return value;
}

byte reverseAll(byte indata)
{
  /*TODO*/
  return value;
}

void setup()
{

  // set pins to output because they are addressed in the main loop
  pinMode(strobePin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  // Initialize strobe low to enable shifting
  digitalWrite(strobePin, LOW);

  Serial.begin(9600);
}

void shiftOut16(int dataPin, int clockPin, unsigned int value)
{

  for (/*TODO*/)
  {

    digitalWrite(clockPin, LOW);

    digitalWrite(dataPin, /*TODO*/);
    digitalWrite(clockPin, HIGH);
    delay(1);
  }

  digitalWrite(clockPin, LOW);
  delay(1);
}

void loop()
{

  for (int j = 0; j < 6; j++)
  {

    unsigned int data = /*TODO*/;

    digitalWrite(strobePin, LOW);

    shiftOut16(dataPin, clockPin, data);

    digitalWrite(strobePin, HIGH); // latch outputs
    delay(1);
    digitalWrite(strobePin, LOW);
  }
}
