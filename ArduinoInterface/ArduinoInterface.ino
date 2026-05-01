// Servicetastatur-Interface
// J3 (top to bottom): VBUS - InST - ClockIn - Enable - STout - GND
// TX (Board to Machine): U13 74HC165 (8-bit PISO, no swap, no pin reversal)
// RX (Machine to Board): U20+U19 74HC4094 Cascade (16-bit SIPO, swap D1/D3 on U19, pins reversed QP7=D0)

const int PIN_TX  = 5;  // Arduino Output: InST / 2STin at U20 (Board RX)
const int PIN_EN  = 2;  // Arduino Input: EnStrobe from machine (INT0)
const int PIN_CLK = 3;  // Arduino Input: Clock from machine (INT1)
const int PIN_RX  = 4;  // Arduino Input: STout from U13 (Board TX)

// TX from board perspective (sniffed by Arduino from U13)
volatile uint16_t txShift   = 0;
volatile uint16_t txLatched = 0;
volatile bool     txReady   = false;

// RX from board perspective (driven by Arduino into U20/U19)
volatile uint16_t rxData   = 0x0000;
volatile int8_t   rxBitPos = 15;

// Helper to reverse bits in a byte
uint8_t reverseByte(uint8_t x) {
  x = ((x & 0x55) << 1) | ((x & 0xAA) >> 1);
  x = ((x & 0x33) << 2) | ((x & 0xCC) >> 2);
  x = (x << 4) | (x >> 4);
  return x;
}

// Process data transmitted by the board (from U13)
uint8_t processTX(uint16_t val) {
  // U13 has no swap and no reversal.
  // After 16 clocks (MSB first), the 8 bits of data end up in the upper byte.
  return (uint8_t)((val >> 8) & 0xFF);
}

// Prepare data for the board to receive (into U19/U20)
uint16_t prepareRX(uint16_t val) {
  uint8_t low  = val & 0xFF;
  uint8_t high = (val >> 8) & 0xFF;

  // 1. D1 and D3 are swapped on U19 (RX path)
  uint8_t d1 = (low >> 1) & 1;
  uint8_t d3 = (low >> 3) & 1;
  low &= ~((1 << 1) | (1 << 3));
  low |= (d3 << 1) | (d1 << 3);

  // 2. Pins are reversed on U19/U20 (QP7=D0)
  low  = reverseByte(low);
  high = reverseByte(high);

  // U19 (low byte) is at the end of the shift chain.
  // Bits are shifted MSB first (bit 15 down to 0).
  // The first 8 bits shifted end up in U19, the next 8 in U20.
  return ((uint16_t)low << 8) | high;
}

void isr_clock() {
  if (digitalRead(PIN_CLK) == HIGH) {
    // Rising edge: read Board TX bit, advance Board RX bit pointer
    txShift = (txShift << 1) | (uint16_t)digitalRead(PIN_RX);
    if (rxBitPos > 0) rxBitPos--;
  } else {
    // Falling edge: output next Board RX bit (stable before next rising edge)
    digitalWrite(PIN_TX, (rxData >> rxBitPos) & 1);
  }
}

void isr_enable() {
  // Rising edge: secure TX frame, reset RX and TX for next frame
  txLatched = txShift;
  txReady   = true;
  txShift   = 0;
  rxBitPos  = 15;
  digitalWrite(PIN_TX, (rxData >> 15) & 1);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TX,  OUTPUT);
  pinMode(PIN_EN,  INPUT);
  pinMode(PIN_CLK, INPUT);
  pinMode(PIN_RX,  INPUT);
  digitalWrite(PIN_TX, (rxData >> 15) & 1);
  attachInterrupt(digitalPinToInterrupt(PIN_EN),  isr_enable, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_CLK), isr_clock,  CHANGE);
}

void loop() {
  if (txReady) {
    noInterrupts();
    uint16_t val = txLatched;
    txReady = false;
    interrupts();

    // Extract raw byte from Board TX stream
    uint8_t txValue = processTX(val);

    Serial.print("TX: 0x");
    if (txValue < 0x10) Serial.print("0");
    Serial.println(txValue, HEX);
  }

  // Receive hex value via Serial to send to Board RX, e.g. "A5"
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length() > 0) {
      uint16_t val  = (uint16_t)strtoul(s.c_str(), nullptr, 16);
      uint16_t prepared = prepareRX(val);
      noInterrupts();
      rxData   = prepared;
      rxBitPos = 15;
      digitalWrite(PIN_TX, (rxData >> 15) & 1);
      interrupts();
      Serial.print("RX: 0x");
      Serial.print(val, HEX);
      Serial.print(" -> prepared: 0x");
      Serial.println(prepared, HEX);
    }
  }
}
