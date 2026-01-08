// Service Buchse von oben nach unten

//GND
const int OutST = 5;
const int EnST = 2;
const int nTaktST = 3;
const int DataST = 4;
//5V

word indata;
word endata;
bool received = false;

void setup() {
  Serial.begin(115200);

  pinMode(OutST, OUTPUT);
  pinMode(EnST, INPUT);
  pinMode(nTaktST, INPUT);
  pinMode(DataST, INPUT);

  attachInterrupt(digitalPinToInterrupt(EnST), enable, CHANGE);
  attachInterrupt(digitalPinToInterrupt(nTaktST), takt, CHANGE);
}

void loop() {
  if (received) {
    Serial.println(endata,HEX);
    received = 0;
  }
}

void enable() {
  endata = indata;
  received = 1;
}

void takt() {
  indata = indata << 1;
  indata = indata | digitalRead(DataST);
}
