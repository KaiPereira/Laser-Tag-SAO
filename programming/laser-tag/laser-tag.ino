// Laser tag blaster for ATtiny84 (ATTinyCore)
//
//   Button    PA2 (physical pin 11) -> GND, uses internal pull-up
//   IR TX     PA5 (physical pin 8)  -> NPN base resistor -> VSLY5940
//   Buzzer    PB0 (physical pin 2)
//   IR RX     PB2 (physical pin 5)  <- 38 kHz IR receiver output (TSOP-style, active LOW)
//
// Set Tools > Clock to "8 MHz (internal)" before uploading. setup() also
// clears the clock divider, so Burn Bootloader isn't strictly needed.
//
// IR safety: each shot is ~10 ms of short bursts at <=50% carrier duty,
// with a forced cooldown between shots and the pin always left LOW.

#include <util/delay.h>
#include <avr/power.h>

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

// Receiver debug: set to 1 and the buzzer beeps for as long as the IR receiver
// sees any 38 kHz IR (a TV remote, another badge, or this badge's own shot
// bouncing back). Normal hit detection is skipped. Set to 0 for real games.
#define DEBUG_RX 0

// The TSOP38438 receiver is a short-burst (AGC4) type: it treats one long
// continuous 38 kHz burst as noise and suppresses it. So a shot is a train of
// short bursts, like a TV remote, and a hit means seeing enough of them in a row.
const uint8_t  SHOT_PULSES     = 8;     // bursts per shot
const uint8_t  PULSE_CYCLES    = 24;    // ~630 us of 38 kHz per burst
#define        PULSE_GAP_US      630    // IR off between bursts

// Shot power: microseconds the LED is ON in each ~26 us carrier cycle.
// 13 = 50% duty (full power, bounces off walls and hits from anywhere).
// Lower = weaker shot, so only a direct hit counts. The receiver still sees
// the 38 kHz carrier down to ~2. Tune this to taste: try 4, then 2-6.
#define        IR_ON_US          4
const uint8_t  HIT_MIN_PULSES  = 4;     // bursts that must be seen to count as a hit
const uint16_t PULSE_MIN_US    = 200;   // receiver LOW pulse width accepted as a burst
const uint16_t PULSE_MAX_US    = 1500;
const uint16_t GAP_MAX_US      = 1500;  // longer silence ends the shot
const uint16_t COOLDOWN_MS     = 300;   // minimum time between shots (no auto-fire)
const uint16_t HIT_LOCKOUT_MS  = 1000;  // one bolt = one hit
const uint16_t SELF_IGNORE_MS  = 50;    // ignore our own shot's reflections

unsigned long lastShot = 0;
unsigned long lastHit = 0;
bool wasPressed = false;

void irOff() {
  PORTA &= ~IR_BIT;
}

// One shot: SHOT_PULSES short 38 kHz bursts with gaps between them.
// Interrupts are paused so the carrier stays clean.
void fireShot() {
  noInterrupts();
  for (uint8_t p = 0; p < SHOT_PULSES; p++) {
    for (uint8_t i = 0; i < PULSE_CYCLES; i++) {
      PORTA |= IR_BIT;
      _delay_us(IR_ON_US);
      PORTA &= ~IR_BIT;
      _delay_us(25 - IR_ON_US);  // ~1 us of loop overhead at 8 MHz
    }
    _delay_us(PULSE_GAP_US);
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

// "You got hit": a crunchy falling zap, then a two-note "you're down" sting.
// About 0.8 s total.
void hitSound() {
  // Falling zap; alternating between f and 3/4 f gives it a gritty, electric edge
  for (int f = 1800; f > 150; f -= 40) {
    tone(BUZZER, ((f / 40) % 2) ? f : f * 3 / 4);
    delay(8);
  }
  noTone(BUZZER);
  delay(60);

  // Two descending notes
  tone(BUZZER, 400);
  delay(120);
  noTone(BUZZER);
  delay(40);
  tone(BUZZER, 250);
  delay(250);

  noTone(BUZZER);
  digitalWrite(BUZZER, LOW);
}

// Returns true if the receiver shows a shot: several short LOW pulses in a row.
// The first pulse can be any length, since we may have started watching midway.
bool checkForBolt() {
  if (digitalRead(IR_RX) == HIGH) return false;  // nothing seen

  uint8_t pulses = 0;
  while (true) {
    // Receiver LOW = burst
    unsigned long start = micros();
    while (digitalRead(IR_RX) == LOW) {
      if (micros() - start > PULSE_MAX_US) {
        // Too long to be one of our bursts: let it clear (bounded) and give up
        unsigned long t = micros();
        while (digitalRead(IR_RX) == LOW && micros() - t < 50000UL) {}
        return false;
      }
    }
    if (pulses > 0 && micros() - start < PULSE_MIN_US) return false;  // glitch
    if (++pulses >= HIT_MIN_PULSES) return true;

    // Receiver HIGH = gap; wait for the next burst
    start = micros();
    while (digitalRead(IR_RX) == HIGH) {
      if (micros() - start > GAP_MAX_US) return false;
    }
  }
}

void setup() {
  // New ATtiny84s ship with the CKDIV8 fuse set (8 MHz / 8 = 1 MHz), which makes
  // every sound 8x slower and deeper and puts the IR carrier at ~5 kHz.
  // Force the full 8 MHz in software so it works even if the fuses were never burned.
  clock_prescale_set(clock_div_1);

  irOff();          // LOW before making it an output, so the LED never blips on
  DDRA |= IR_BIT;
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(IR_RX, INPUT_PULLUP);  // most receivers have one built in; this doesn't hurt
}

void loop() {
#if DEBUG_RX
  static bool beeping = false;
  static unsigned long lastSeen = 0;
  if (digitalRead(IR_RX) == LOW) lastSeen = millis();
  bool seeing = (millis() - lastSeen < 100);  // stretch short shots so you can hear them
  if (seeing && !beeping) tone(BUZZER, 2000);
  if (!seeing && beeping) { noTone(BUZZER); digitalWrite(BUZZER, LOW); }
  beeping = seeing;
  // One shot per press (no pew, so you only hear the receiver)
  bool dbgPressed = (digitalRead(BUTTON) == LOW);
  if (dbgPressed && !wasPressed && millis() - lastShot >= COOLDOWN_MS) {
    lastShot = millis();
    fireShot();
  }
  wasPressed = dbgPressed;
  return;
#endif

  unsigned long now = millis();
  bool pressed = (digitalRead(BUTTON) == LOW);

  // Fire once per press, and only after the cooldown
  if (pressed && !wasPressed && (now - lastShot >= COOLDOWN_MS)) {
    lastShot = now;
    fireShot();  // IR first, so the buzzer's timer doesn't disturb it
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
