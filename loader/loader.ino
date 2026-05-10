#include "rom.hpp"

char loader[8];
/*
8 byte initial instructions for the CPU.
generated from `loader.asm` with `make`.
*/
const char INITIAL_LOADER[8] = "\x21\x00\x00\x7e\x23\xc3\x03\x00";

// define the purpuse of various pins
#define Z80_RST 10
#define Z80_CLK 12
#define COM_RE A4
#define COM_WE A5
#define COM_CS 11
#define DATA_START 2

// various other variables
#define CS_ARDUINO HIGH
#define CS_RAM LOW

// clock delays
unsigned long PULSE_DELAY = 2;
unsigned long PULSE_WAIT = 2;

const unsigned int FPS = 16;

inline void delayPulseWait() { delayMicroseconds(PULSE_WAIT); }
inline void delayPulseDelay() { delayMicroseconds(PULSE_DELAY); }

// helper code to pulse a pin
void digitalPulse(int pin, int signal) {
  delayPulseWait();
  digitalWrite(pin, signal);
  delayPulseDelay();
  digitalWrite(pin, !signal);
  delayPulseWait();
}

// assigns the 8 data pins a given pinmode
void dataPinMode(int pinmode) {
  for (int bit = 0; bit < 8; bit++)
    pinMode(DATA_START + bit, pinmode);
}

// assigns the 3 address pins a certain pinmode as well as the chip select
void addressPinMode(int pinmode) {
  for (int bit = 0; bit < 3; bit++)
    pinMode(addressPin(bit), pinmode);
  pinMode(COM_CS, pinmode);
}

// write a byte to the data lines
void writeByte(uint8_t byte) {
  for (int bit = 0; bit < 8; bit++)
    digitalWrite(DATA_START + bit, bitRead(byte, bit));
}

// read and return a byte from the data lines
uint8_t readByte() {
  uint8_t byte = 0;
  for (int bit = 0; bit < 8; bit++)
    byte |= digitalRead(DATA_START + bit) << bit;
  return byte;
}

// get the pin number of the ith addr pin
constexpr int addressPin(int bit) { return A2 - bit; }

// read the addr from the addr pins
int8_t readAddress() {
  int8_t res = 0;
  for (int bit = 0; bit < 3; bit++)
    res |= digitalRead(addressPin(bit)) << bit;
  return res;
}

void die() {
  while (1)
    delay(1000);
}

// write loader program, then verifies it
void writeLoader() {
  // set necessary pinmodes to output
  addressPinMode(OUTPUT);
  dataPinMode(OUTPUT);
  pinMode(COM_RE, OUTPUT);
  pinMode(COM_WE, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // loops until the loader program is verified
  while (1) {
    // sets data pins to output
    dataPinMode(OUTPUT);

    // disables all pins
    digitalWrite(COM_RE, HIGH);
    digitalWrite(COM_WE, HIGH);
    // enables chip select
    digitalWrite(COM_CS, CS_RAM);

    // writes the loader program
    for (int i = 0; i < 8; i++) {
      for (int bit = 0; bit < 3; bit++)
        // sets correct addr
        digitalWrite(addressPin(bit), i >> bit & 1);
      // writes the byte
      writeByte((uint8_t)loader[i]);
      // pulses the WE pin so that the ram writes the data into its memory
      digitalPulse(COM_WE, LOW);
    }
    // sets all data pins to input
    dataPinMode(INPUT);
    // if loader program is verified, break
    // otherwise rewrite the loader program until it works
    if (verifyLoader())
      break;
  }
  // reset the pinmodes
  addressPinMode(INPUT);
  dataPinMode(INPUT);
  pinMode(COM_RE, INPUT);
  pinMode(COM_WE, INPUT);
}

// verifies the loader program has been correctly loaded into ram
bool verifyLoader() {
  // enables chip select
  digitalWrite(COM_CS, CS_RAM);
  // disables all pins
  digitalWrite(COM_RE, HIGH);
  digitalWrite(COM_WE, HIGH);

  int valid = true;

  // loops through loader program to verify it has been loaded correctly
  for (int i = 0; i < 8; i++) {
    // sets all addr pins correctly
    for (int bit = 0; bit < 3; bit++)
      digitalWrite(addressPin(bit), i >> bit & 1);
    delayPulseWait();
    // signals the ram to output the data at the given byte
    digitalWrite(COM_RE, LOW);
    delayPulseDelay();
    // reads the data
    uint8_t read = readByte();
    // gets what the expected byte is
    uint8_t expected = loader[i];
    // if the read and expected data do not match, mark as invalid
    if (expected != read)
      valid = false;

    digitalWrite(COM_RE, HIGH);
    delayPulseWait();
  }

  // Serial.print("loader verification ");
  // if(valid){
  //   Serial.println("passed");
  // }else {
  //   Serial.println("failed");
  // }

  // returns whether the loader program is valid
  return valid;
}

void disableCpu() {
  digitalWrite(Z80_RST, LOW);
  digitalWrite(Z80_CLK, HIGH);
  delay(10);

  // pulses clock an arbitray number of times (>3 as per datasheet) to ensure
  // the CPU is finished its reset cycle
  Serial.print("pulsing clock...");
  for (int i = 0; i < 20; i++)
    digitalPulse(Z80_CLK, HIGH);
  Serial.println(" done");
  digitalWrite(Z80_CLK, LOW);
}

void enableCpu() { digitalWrite(Z80_RST, HIGH); }

long clockCycle = 0;

int8_t prevReadAddr = -1;

unsigned long nextFrameTime = 0;

// setup function which runs when the arduino starts
void setup() {
  Serial.begin(115200);

  // sets data pins, reset and clock to the correct pinmodes
  pinMode(Z80_RST, OUTPUT);
  pinMode(Z80_CLK, OUTPUT);
  pinMode(COM_RE, INPUT);
  pinMode(COM_WE, INPUT);
  pinMode(COM_CS, INPUT);
  dataPinMode(INPUT);
  addressPinMode(INPUT);

  // disable cpu
  disableCpu();

  // write loader code
  Serial.print("writing loader...");
  memcpy(loader, INITIAL_LOADER, sizeof(loader));
  writeLoader();
  Serial.println(" done");

  const int PROGRESS_WIDTH = 64;

  // enable cpu
  enableCpu();

  Serial.print("writing ");
  Serial.print(sizeof(FULL_CODE) - 8);
  Serial.println(" bytes of main code...");
  Serial.print(".");
  for (int i = 0; i < PROGRESS_WIDTH; i++) {
    Serial.print("_");
  }
  Serial.println(".");

  // pulse the clock until the cpu enters the main loop
  for (int i = 0; i < 17; i++) {
    digitalPulse(Z80_CLK, HIGH);
  }

  Serial.print("[");
  int prev_indicator_pos = 0;
  // main loader loop!
  for (uint16_t addr = 0; addr < sizeof(FULL_CODE); addr++) {
    int8_t curAddr = readAddress();
    if (curAddr != (addr & 7)) {
      Serial.print("invalid addr encountered! expected ");
      Serial.print((int)addr & 7);
      Serial.print(", got ");
      Serial.println(curAddr);
      die();
    }
    // it is expected that the CPU is putting the required addr on the addr
    // lines, verify it
    if (digitalRead(COM_RE)) {
      Serial.println("expected cpu to read on this clock cycle!");
      Serial.print("addr: ");
      Serial.println(addr);
      die();
    }

    uint8_t code_value = pgm_read_byte_near(&FULL_CODE[addr]);

    // only override the addr if it would not otherwise override the loader code
    if (addr >= 8) {
      // override the CPU's RE and WE; because there are resistors on the CPU
      // side this is fine
      pinMode(COM_RE, OUTPUT);
      pinMode(COM_WE, OUTPUT);
      digitalWrite(COM_RE, HIGH);
      digitalWrite(COM_WE, HIGH);
      dataPinMode(OUTPUT);
      writeByte(code_value);
      // and write the data!
      digitalPulse(COM_WE, LOW);
      pinMode(COM_RE, INPUT);
      pinMode(COM_WE, INPUT);
      dataPinMode(INPUT);
    }

    // the cpu reads twice; use this to verify that the previous data was
    // written correctly
    digitalPulse(Z80_CLK, HIGH);
    uint8_t expected_value = addr < 8 ? loader[addr] : code_value;
    uint8_t actual_value = readByte();

    if (actual_value != expected_value) {
      Serial.print("invalid data encountered at ");
      Serial.print(addr);
      Serial.print(": expected ");
      Serial.print(expected_value);
      Serial.print(", got ");
      Serial.println(actual_value);
      die();
    }

    // advance to the next cycle
    for (int i = 0; i < 22; i++)
      digitalPulse(Z80_CLK, HIGH);

    int new_indicator_pos =
        (long)(addr + 1) * PROGRESS_WIDTH / sizeof(FULL_CODE);

    // loading indicator
    for (; prev_indicator_pos < new_indicator_pos; prev_indicator_pos++) {
      Serial.print("=");
    }
  }
  Serial.println("]");

  disableCpu();

  Serial.print("writing first 8 bytes...");
  // memcpy(loader, FULL_CODE, sizeof(loader)); doesn't work because FULL_CODE
  // is PROGMEM
  for (int i = 0; i < 8; i++)
    loader[i] = pgm_read_byte_near(&FULL_CODE[i]);

  writeLoader();
  Serial.println(" done!");

  enableCpu();

  Serial.println("setup complete. fingers crossed!");

  Serial.println("starting program...");

  nextFrameTime = micros();

  PULSE_DELAY = PULSE_WAIT = 1UL;
}

// converts an integer (0 to 15) to the respective hex character
char hexToChar(uint8_t c) {
  if (c <= 9)
    return c + '0';
  else
    return c - 10 + 'a';
}

char fb_queue[256];
uint8_t fbptr = 0;

inline void clockPulse() {
  u8 readPort = PORTB;
  PORTB = readPort | (1 << Z80_CLK - 8);

  __asm__("nop");
  __asm__("nop");
  __asm__("nop");
  __asm__("nop");
  __asm__("nop");

  PORTB = readPort;
}
inline byte readRwPins() {
  return digitalRead(COM_WE) << 1 | digitalRead(COM_RE);
  // return PINC >> COM_RE - 16 & 0b11;
}
inline bool readCs() { return PINB & 1 << COM_CS - 8; }

// main loop program which runs after setup finishes
void loop() {
  clockPulse();

  bool arduino_enabled = readCs() == CS_ARDUINO;
  byte rw;
  if (!arduino_enabled || (rw = readRwPins()) == 0b11) {
    if (prevReadAddr != -1) {
      // set data pin modes to input once the cpu has finished reading
      dataPinMode(INPUT);
      prevReadAddr = -1;
    }
    return;
  }

  if ((rw & 0b10) == 0) {
    int8_t addr = readAddress();
    if (addr == 0) {
      fb_queue[fbptr++] = readByte();
    } else if (addr == 1) {
      Serial.write('\e');
      Serial.write(fb_queue, sizeof(fb_queue));
      fbptr = 0;
    } else {
      Serial.print("warning: writing to address ");
      Serial.print(addr);
      Serial.print(" ( data ");
      Serial.print(readByte());
      Serial.println(") not implemented");
    }
  } else if ((rw & 0b01) == 0) {
    int8_t addr = readAddress();
    if (prevReadAddr == addr) {
      // pins are already set; no need to re-output to the data lines
      return;
    }
    prevReadAddr = addr;

    dataPinMode(OUTPUT);

    uint8_t output_byte = 0x76;
    if (addr == 0) {
      // key available
      output_byte = Serial.available() > 0;
    } else if (addr == 1) {
      int input_key = -1;
      while (Serial.available()) {
        input_key = Serial.read();
        // special case: reset everything if we signaled 'r'
        if (input_key == 'r') {
          setup();
          return;
        }
      }
      output_byte = input_key;
    } else {
      Serial.print("warning: reading from address ");
      Serial.print(addr);
      Serial.println(" not implemented");
    }
    writeByte(output_byte);
  } else {
    Serial.println("unreachable");
    die();
  }
}
