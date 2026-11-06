// Laser tag blaster for ATtiny84 (ATTinyCore)
//
//   Button    PA2 (physical pin 11) -> GND, uses internal pull-up
//   IR TX     PA5 (physical pin 8)  -> NPN base resistor -> VSLY5940
//   Buzzer    PB0 (physical pin 2)
//   IR RX     PB2 (physical pin 5)  <- 38 kHz IR receiver output (TSOP-style, active LOW)
//
// Set Tools > Clock to "8 MHz (internal)" and Burn Bootloader once
// before uploading, so the 38 kHz IR carrier timing is right.
//
// IR safety: the LED only gets a 10 ms burst at 50% duty per shot,
// with a forced cooldown between shots and the pin always left LOW.

#include <util/delay.h>

#ifdef PIN_PA2
  #define BUTTON   PIN_PA2
  #define BUZZER   PIN_PB0
  #define IR_RX    PIN_PB2
#else
  #define BUTTON   2    // ATTinyCore default (counterclockwise) pinmap
  #define BUZZER   10
  #define IR_RX    8
#endif

#define IR_BIT (1 << 5)  // PA5, driven directly on PORTA for accurate timing

const uint16_t IR_BURST_MS     = 10;    // IR "shot" length
const uint16_t COOLDOWN_MS     = 300;   // minimum time between shots (no auto-fire)
const uint16_t HIT_MIN_US      = 3000;  // receiver must see IR this long to count as a hit
const uint16_t HIT_MAX_US      = 30000; // longer than this is not a bolt (sunlight, remote, etc.)
const uint16_t HIT_LOCKOUT_MS  = 1000;  // one bolt = one hit
const uint16_t SELF_IGNORE_MS  = 50;    // ignore our own shot's reflections

unsigned long lastShot = 0;
unsigned long lastHit = 0;
bool wasPressed = false;

void irOff() {
  PORTA &= ~IR_BIT;
}

// 38 kHz carrier burst. Interrupts are paused so the carrier stays clean.
void irBurst(uint16_t ms) {
  uint16_t cycles = ms * 38;
  noInterrupts();
  for (uint16_t i = 0; i < cycles; i++) {
    PORTA |= IR_BIT;
    _delay_us(13);
    PORTA &= ~IR_BIT;
    _delay_us(12);  // ~1 us of loop overhead at 8 MHz
  }
  irOff();
  interrupts();
}

// Descending "pew" laser sound, about 130 ms.
void pew() {
  for (int f = 3000; f > 400; f -= 100) {
    tone(BUZZER, f);
    delay(5);
  }
  noTone(BUZZER);
  digitalWrite(BUZZER, LOW);
}

// Low warbling "you got hit" buzz, about 400 ms.
void hitSound() {
  for (int i = 0; i < 8; i++) {
    tone(BUZZER, (i % 2) ? 180 : 140);
    delay(50);
  }
  noTone(BUZZER);
  digitalWrite(BUZZER, LOW);
}

// Returns true if the receiver output stays LOW for a bolt-length pulse.
bool checkForBolt() {
  if (digitalRead(IR_RX) == HIGH) return false;  // nothing seen

  unsigned long start = micros();
  while (digitalRead(IR_RX) == LOW) {
    if (micros() - start > HIT_MAX_US) {
      // Way too long: noise or constant IR. Wait for it to clear, don't count it.
      while (digitalRead(IR_RX) == LOW) {}
      return false;
    }
  }
  return (micros() - start) >= HIT_MIN_US;
}

void setup() {
  irOff();          // LOW before making it an output, so the LED never blips on
  DDRA |= IR_BIT;
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(IR_RX, INPUT_PULLUP);  // most receivers have one built in; this doesn't hurt
}

void loop() {
  unsigned long now = millis();
  bool pressed = (digitalRead(BUTTON) == LOW);

  // Fire once per press, and only after the cooldown
  if (pressed && !wasPressed && (now - lastShot >= COOLDOWN_MS)) {
    lastShot = now;
    irBurst(IR_BURST_MS);  // IR first, so the buzzer's timer doesn't disturb it
    pew();
  }
  wasPressed = pressed;

  // Hit detection, skipping right after our own shot and during the hit lockout
  if ((millis() - lastShot >= SELF_IGNORE_MS) &&
      (millis() - lastHit >= HIT_LOCKOUT_MS) &&
      checkForBolt()) {
    lastHit = millis();
    hitSound();
  }

  irOff();   // belt and braces: IR is always off between shots
  delay(1);  // short loop so a 10 ms bolt is never missed
}
