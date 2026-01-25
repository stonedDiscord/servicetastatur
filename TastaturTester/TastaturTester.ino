/*

Sending data through 2 shift registers 74HC4094

 */
//Pin connected to STR of 74HC4094
int strobePin = 8;
//Pin connected to CP of 74HC4094
int clockPin = 12;
//Pin connected to D of 74HC4094
int dataPin = 11;


unsigned int dataArray[13] = {
  0x0F80,  // reg 7, bit 7 = 1  (arm / acknowledge)
  0x0F00,  // reg 7, bit 7 = 0  (exit service mode)

  0x4100, 0x8100,  // H
  0x4100, 0x5100,  // E
  0x4100, 0xC100,  // L
  0x4100, 0xC100,  // L
  0x4100, 0xF100   // O
};


/*
unsigned int dataArray[13] = {
0x1111,
0x2222,
0x3333,
0x4444,
0x5555,
0x1111,
0x2222,
0x3333,
0x4444,
0x5555,
0x1111,
0x2222,
0x3333  
};
*/

unsigned int stupidSwap(unsigned int value) {
  // Extract upper byte (DAT_EXTMEM_0050)
  unsigned int upper = (value >> 8) & 0xFF;
  unsigned int lower = value & 0xFF;

  // Extract bits 1 and 3
  unsigned int bit1 = (upper >> 1) & 1;
  unsigned int bit3 = (upper >> 3) & 1;

  // Clear bits 1 and 3
  upper &= ~((1 << 1) | (1 << 3));

  // Swap them
  upper |= (bit1 << 3);
  upper |= (bit3 << 1);

  // Reassemble 16-bit word
  return (upper << 8) | lower;
}

void setup() {

  //set pins to output because they are addressed in the main loop
  pinMode(strobePin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  // Initialize strobe low to enable shifting
  digitalWrite(strobePin, LOW);

  Serial.begin(9600);

}

void shiftOut16(int dataPin, int clockPin, unsigned int value) {

  for (int i = 15; i >= 0; i--) {

    digitalWrite(clockPin, LOW);

    digitalWrite(dataPin, (value & (1 << i)) ? HIGH : LOW);
    digitalWrite(clockPin, HIGH);
    delay(1);
  }

  digitalWrite(clockPin, LOW);
  delay(1);
}

void loop() {

  for (int j = 0; j < 13; j++) {

    //unsigned int data = stupidSwap(dataArray[j]);
    unsigned int data = dataArray[j];

    digitalWrite(strobePin, LOW);

    shiftOut16(dataPin, clockPin, data);

    digitalWrite(strobePin, HIGH);   // latch outputs
    delay(1);
    digitalWrite(strobePin, LOW);
  }
}
