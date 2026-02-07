// Midi Expression — USB MIDI pedal controller
// Teensy 4.1 (USB Type: "MIDI" in Arduino IDE / Tools menu)
// Control Surface library by tttapa
//
// Three 6-pin switched 1/4" TRS sockets:
//   J1 — Sustain  (Yamaha FC3A, expression)  CC 64  Channel 5
//   J2 — Soft     (On Stage KSP, on/off)     CC 67  Channel 5
//   J3 — Sostenuto(Casio SP20, on/off)       CC 66  Channel 5
//
// See SCHEMATIC.md for full wiring details.

#include <Control_Surface.h>

// ---------------------------------------------------------------------------
// MIDI interface — Teensy native USB MIDI
// ---------------------------------------------------------------------------
USBMIDI_Interface midi;

// ---------------------------------------------------------------------------
// Pin definitions (from SCHEMATIC.md)
// ---------------------------------------------------------------------------

// J1 — expression-capable socket (Sustain / Yamaha FC3A)
constexpr pin_t J1_TIP_PIN    = A0;  // pin 14 — analog, pedal wiper
constexpr pin_t J1_RING_PIN   = A1;  // pin 15 — analog, VCC sense
constexpr pin_t J1_DETECT_PIN = 0;   // pin 0  — S_s plug detect

// J2 — expression-capable socket (Soft / On Stage KSP)
constexpr pin_t J2_TIP_PIN    = A2;  // pin 16 — analog, pedal wiper
constexpr pin_t J2_RING_PIN   = A3;  // pin 17 — analog, VCC sense
constexpr pin_t J2_DETECT_PIN = 1;   // pin 1  — S_s plug detect

// J3 — digital-only socket (Sostenuto / Casio SP20)
constexpr pin_t J3_TIP_PIN    = 2;   // pin 2  — digital, pedal input
constexpr pin_t J3_DETECT_PIN = 3;   // pin 3  — S_s plug detect

// ---------------------------------------------------------------------------
// Expression pedal wiper configuration
//
// Different expression pedals put the wiper (variable voltage) on
// different TRS contacts:
//   TIP  — wiper on Tip, VCC/GND on Ring  (e.g. Yamaha FC3A)
//   RING — wiper on Ring, VCC/GND on Tip  (some other manufacturers)
//
// Set each jack to match the pedal you are plugging in.
// The non-wiper pin is used as the "sense" pin for TS/TRS detection.
// ---------------------------------------------------------------------------
enum class WiperPin : uint8_t { TIP, RING };

constexpr WiperPin J1_WIPER = WiperPin::TIP;   // Yamaha FC3A: wiper on Tip
constexpr WiperPin J2_WIPER = WiperPin::TIP;   // change to RING if needed

// ---------------------------------------------------------------------------
// MIDI addresses — all on Channel 5
// ---------------------------------------------------------------------------
constexpr Channel MIDI_CH = Channel_5;

// CC numbers per MIDI standard
constexpr MIDIAddress CC_SUSTAIN   = {MIDI_CC::Damper_Pedal, MIDI_CH}; // CC 64
constexpr MIDIAddress CC_SOFT      = {MIDI_CC::Soft_Pedal,   MIDI_CH}; // CC 67
constexpr MIDIAddress CC_SOSTENUTO = {MIDI_CC::Sostenuto,    MIDI_CH}; // CC 66

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------
constexpr unsigned long DETECT_INTERVAL_MS   = 50;   // plug-detect poll rate
constexpr unsigned long DEBOUNCE_SETTLE_MS   = 200;  // settle time after plug event
constexpr unsigned long RING_SAMPLE_COUNT    = 8;     // samples for TS/TRS decision
constexpr uint16_t      RING_THRESHOLD       = 200;   // ADC threshold: below = TS plug
constexpr uint16_t      EXPRESSION_NOISE     = 2;     // dead-band for expression jitter

// ---------------------------------------------------------------------------
// Control Surface MIDI output elements
//
// We create them with placeholder pins / addresses and manage them manually
// because plugs can be inserted and removed at runtime and the pedal type
// on J1/J2 is not known until a plug is detected.
// ---------------------------------------------------------------------------

// J1 sustain — one CCPotentiometer per possible wiper pin.
// Only the one matching J1_WIPER is enabled when expression is detected.
CCPotentiometer j1ExprTip  {J1_TIP_PIN,  CC_SUSTAIN};
CCPotentiometer j1ExprRing {J1_RING_PIN, CC_SUSTAIN};

// J2 soft — same approach.
CCPotentiometer j2ExprTip  {J2_TIP_PIN,  CC_SOFT};
CCPotentiometer j2ExprRing {J2_RING_PIN, CC_SOFT};

// J3 sostenuto — digital only, always on/off.
// CCButton sends 0x7F on press, 0x00 on release.
CCButton j3Button {J3_TIP_PIN, CC_SOSTENUTO};

// ---------------------------------------------------------------------------
// Per-jack runtime state
// ---------------------------------------------------------------------------
enum class PedalType : uint8_t {
  UNKNOWN,
  EXPRESSION,  // TRS plug — analog expression pedal
  SWITCH_NO,   // TS plug  — normally-open switch
  SWITCH_NC,   // TS plug  — normally-closed switch
};

struct JackState {
  bool       plugged       = false;
  bool       wasPlugged    = false;
  PedalType  type          = PedalType::UNKNOWN;
  bool       enabled       = false;   // is the CS element enabled?
  unsigned long settleEnd  = 0;       // debounce after plug event
};

JackState j1State, j2State, j3State;

// Last sent CC values (to avoid duplicate sends)
uint8_t j1LastSent = 0xFF;
uint8_t j2LastSent = 0xFF;
uint8_t j3LastSent = 0xFF;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
bool readDetect(pin_t pin);
PedalType classifyJack(pin_t sensePin, pin_t wiperPin);
PedalType classifyDigitalJack(pin_t tipPin);
void handleExpressionJack(JackState &st, pin_t detectPin, pin_t tipPin,
                          pin_t ringPin, WiperPin wiperCfg,
                          CCPotentiometer &exprTip, CCPotentiometer &exprRing,
                          const MIDIAddress &addr, uint8_t &lastSent);
void handleDigitalJack(JackState &st, pin_t detectPin, pin_t tipPin,
                       CCButton &btn, uint8_t &lastSent);
void sendCC(const MIDIAddress &addr, uint8_t value);

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
  // 12-bit ADC for finer expression control
  analogReadResolution(12);

  // Plug-detect pins — INPUT_PULLUP (S_s pins)
  pinMode(J1_DETECT_PIN, INPUT_PULLUP);
  pinMode(J2_DETECT_PIN, INPUT_PULLUP);
  pinMode(J3_DETECT_PIN, INPUT_PULLUP);

  // Disable all MIDI elements until we detect plugs
  j1ExprTip.disable();
  j1ExprRing.disable();
  j2ExprTip.disable();
  j2ExprRing.disable();
  j3Button.disable();

  Control_Surface.begin();
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop() {
  // Let Control Surface handle its internal updates (MIDI I/O, enabled
  // elements, etc.)
  Control_Surface.loop();

  // Plug-detect and pedal management at a modest rate to avoid thrashing
  static unsigned long lastDetect = 0;
  unsigned long now = millis();
  if (now - lastDetect < DETECT_INTERVAL_MS)
    return;
  lastDetect = now;

  handleExpressionJack(j1State, J1_DETECT_PIN, J1_TIP_PIN, J1_RING_PIN,
                       J1_WIPER, j1ExprTip, j1ExprRing,
                       CC_SUSTAIN, j1LastSent);
  handleExpressionJack(j2State, J2_DETECT_PIN, J2_TIP_PIN, J2_RING_PIN,
                       J2_WIPER, j2ExprTip, j2ExprRing,
                       CC_SOFT, j2LastSent);
  handleDigitalJack(j3State, J3_DETECT_PIN, J3_TIP_PIN,
                    j3Button, j3LastSent);
}

// ---------------------------------------------------------------------------
// Plug-detect: S_s reads HIGH when a plug is inserted
// ---------------------------------------------------------------------------
bool readDetect(pin_t pin) {
  return digitalRead(pin) == HIGH;
}

// ---------------------------------------------------------------------------
// Classify an expression-capable jack (J1 or J2) as TRS or TS, and if TS
// determine switch polarity (NO vs NC).
//
// sensePin is the non-wiper pin (used to distinguish TS from TRS):
//   - TRS plug: sensePin connects to VCC/GND end of pot → reads high
//   - TS plug:  sleeve spans Ring+Sleeve contacts → sensePin reads ≈ 0
//
// wiperPin is the pin carrying the expression signal (also used to read
// the switch state when a TS plug is detected).
// ---------------------------------------------------------------------------
PedalType classifyJack(pin_t sensePin, pin_t wiperPin) {
  // Average several sense-pin readings to reject transient noise
  uint32_t senseSum = 0;
  for (uint16_t i = 0; i < RING_SAMPLE_COUNT; i++) {
    senseSum += analogRead(sensePin);
  }
  uint16_t senseAvg = senseSum / RING_SAMPLE_COUNT;

  if (senseAvg < RING_THRESHOLD) {
    // TS plug detected — determine polarity from the wiper pin
    uint16_t wiperVal = analogRead(wiperPin);
    // With pull-up and switch open, wiper pin reads high.
    // If it reads low now, the switch is closed at rest → NC.
    if (wiperVal < 2048) {
      return PedalType::SWITCH_NC;
    }
    return PedalType::SWITCH_NO;
  }

  return PedalType::EXPRESSION;
}

// ---------------------------------------------------------------------------
// Classify J3 (digital-only) switch polarity
// ---------------------------------------------------------------------------
PedalType classifyDigitalJack(pin_t tipPin) {
  // Tip has INPUT_PULLUP via the Control Surface CCButton or manual config.
  // Read the raw pin to determine resting state.
  int val = digitalRead(tipPin);
  // Normally-open: resting HIGH (pull-up). Normally-closed: resting LOW.
  return (val == LOW) ? PedalType::SWITCH_NC : PedalType::SWITCH_NO;
}

// ---------------------------------------------------------------------------
// Send a CC message directly via the MIDI interface
// ---------------------------------------------------------------------------
void sendCC(const MIDIAddress &addr, uint8_t value) {
  midi.sendControlChange(addr, value);
}

// ---------------------------------------------------------------------------
// Handle an expression-capable jack (J1 or J2)
//
// wiperCfg selects which TRS contact carries the expression wiper:
//   TIP  → exprTip is the active CCPotentiometer, Ring is the sense pin
//   RING → exprRing is the active CCPotentiometer, Tip is the sense pin
// ---------------------------------------------------------------------------
void handleExpressionJack(JackState &st, pin_t detectPin, pin_t tipPin,
                          pin_t ringPin, WiperPin wiperCfg,
                          CCPotentiometer &exprTip, CCPotentiometer &exprRing,
                          const MIDIAddress &addr, uint8_t &lastSent) {
  // Derive sense/wiper roles from config
  pin_t wiperAnalogPin = (wiperCfg == WiperPin::TIP) ? tipPin  : ringPin;
  pin_t senseAnalogPin = (wiperCfg == WiperPin::TIP) ? ringPin : tipPin;
  CCPotentiometer &activeExpr = (wiperCfg == WiperPin::TIP) ? exprTip : exprRing;
  CCPotentiometer &otherExpr  = (wiperCfg == WiperPin::TIP) ? exprRing : exprTip;

  unsigned long now = millis();
  bool plugged = readDetect(detectPin);

  // --- Plug event: just inserted or removed ---
  if (plugged != st.wasPlugged) {
    st.wasPlugged = plugged;
    st.settleEnd = now + DEBOUNCE_SETTLE_MS;
    st.plugged = false;        // not yet settled
    st.type = PedalType::UNKNOWN;

    // Disable elements immediately on any plug event
    if (st.enabled) {
      activeExpr.disable();
      st.enabled = false;
    }
    otherExpr.disable();  // always keep the other one off

    // If unplugged, send a zero to clear the controller state
    if (!plugged) {
      sendCC(addr, 0);
      lastSent = 0;
    }
    return;
  }

  // --- Waiting for settle ---
  if (st.settleEnd != 0 && now < st.settleEnd)
    return;

  // --- Settle complete, classify the pedal ---
  if (st.settleEnd != 0 && now >= st.settleEnd) {
    st.settleEnd = 0;
    if (!plugged) {
      st.plugged = false;
      return;
    }
    st.plugged = true;
    st.type = classifyJack(senseAnalogPin, wiperAnalogPin);
  }

  if (!st.plugged)
    return;

  // --- Expression pedal (TRS) — let Control Surface handle it ---
  if (st.type == PedalType::EXPRESSION) {
    if (!st.enabled) {
      activeExpr.enable();
      st.enabled = true;
    }
    // Control_Surface.loop() already reads the pot and sends CC.
    return;
  }

  // --- On/off switch (TS) — read manually as digital ---
  // Disable the CCPotentiometer since we're treating this as a switch.
  if (st.enabled) {
    activeExpr.disable();
    st.enabled = false;
  }

  // Read the wiper pin as digital (the 1 kΩ + pull-up makes this fine)
  int raw = digitalRead(wiperAnalogPin);

  // Apply polarity: NO → pressed = LOW; NC → pressed = HIGH
  bool pressed;
  if (st.type == PedalType::SWITCH_NC)
    pressed = (raw == HIGH);
  else
    pressed = (raw == LOW);

  uint8_t value = pressed ? 0x7F : 0x00;
  if (value != lastSent) {
    sendCC(addr, value);
    lastSent = value;
  }
}

// ---------------------------------------------------------------------------
// Handle the digital-only jack (J3)
// ---------------------------------------------------------------------------
void handleDigitalJack(JackState &st, pin_t detectPin, pin_t tipPin,
                       CCButton &btn, uint8_t &lastSent) {
  unsigned long now = millis();
  bool plugged = readDetect(detectPin);

  // --- Plug event ---
  if (plugged != st.wasPlugged) {
    st.wasPlugged = plugged;
    st.settleEnd = now + DEBOUNCE_SETTLE_MS;
    st.plugged = false;
    st.type = PedalType::UNKNOWN;

    if (st.enabled) {
      btn.disable();
      st.enabled = false;
    }

    if (!plugged) {
      sendCC(CC_SOSTENUTO, 0);
      lastSent = 0;
    }
    return;
  }

  // --- Waiting for settle ---
  if (st.settleEnd != 0 && now < st.settleEnd)
    return;

  // --- Settle complete ---
  if (st.settleEnd != 0 && now >= st.settleEnd) {
    st.settleEnd = 0;
    if (!plugged) {
      st.plugged = false;
      return;
    }
    st.plugged = true;
    // Configure Tip as input with pull-up before classifying
    pinMode(tipPin, INPUT_PULLUP);
    delay(5);  // brief settle for pull-up
    st.type = classifyDigitalJack(tipPin);
  }

  if (!st.plugged)
    return;

  // Read Tip and apply polarity
  int raw = digitalRead(tipPin);
  bool pressed;
  if (st.type == PedalType::SWITCH_NC)
    pressed = (raw == HIGH);
  else
    pressed = (raw == LOW);

  uint8_t value = pressed ? 0x7F : 0x00;
  if (value != lastSent) {
    sendCC(CC_SOSTENUTO, value);
    lastSent = value;
  }
}
