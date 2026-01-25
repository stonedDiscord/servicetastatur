/*

Sending data through 2 shift registers 74HC4094

 */
//Pin connected to STR of 74HC4094
int strobePin = 8;
//Pin connected to CP of 74HC4094
int clockPin = 12;
//Pin connected to D of 74HC4094
int dataPin = 11;

//holders for information you're going to pass to shifting function
unsigned int data;

unsigned int dataArray[10];

void setup() {

  //set pins to output because they are addressed in the main loop
  pinMode(strobePin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  Serial.begin(9600);

  dataArray[0] = 0x6666;
  dataArray[1] = 0x7777;
  dataArray[2] = 0x1234;
  dataArray[3] = 0x5678;
  dataArray[4] = 0x9ABC;
  dataArray[5] = 0xDEF0;
  dataArray[6] = 0x1111;
  dataArray[7] = 0x2222;
  dataArray[8] = 0x3333;
  dataArray[9] = 0x4444;
}

void loop() {

  for (int j = 0; j < 10; j++) {

    //load the data you want from array
    data = dataArray[j];

    digitalWrite(strobePin, 1);

    //move 'em out
    shiftOut(dataPin, clockPin, data);

    digitalWrite(strobePin, 0);
    delay(600);
  }
}

void shiftOut(int myDataPin, int myClockPin, unsigned int myDataOut) {

  int i=0;
  int pinState;
  digitalWrite(myDataPin, 0);
  digitalWrite(myClockPin, 0);

  for (i=15; i>=0; i--)  {
    digitalWrite(myClockPin, 0);
    if ( myDataOut & (1<<i) ) {
      pinState= 1;
    }
    else {
      pinState= 0;
    }

    digitalWrite(myDataPin, pinState);

    digitalWrite(myClockPin, 1);

    digitalWrite(myDataPin, 0);

  }

  digitalWrite(myClockPin, 0);
}
