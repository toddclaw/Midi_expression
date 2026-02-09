// Midi Expression — USB MIDI pedal controller
// Teensy 4.1 (USB Type: "MIDI" in Arduino IDE / Tools menu)
// Control Surface library by tttapa
//
// Three 6-pin switched 1/4" TRS sockets:
//   J1 — Sustain  (Yamaha FC3A, expression)  CC 64  Channel 5
//   J2 — Soft     (On Stage KSP, on/off)     CC 67  Channel 5
//   J3 — Sostenuto(Casio SP20, on/off)       CC 66  Channel 5
//
// 128x64 SSD1306 OLED on I2C (SDA=18, SCL=19) displays live status.
// MIDI CC number and channel are runtime-configurable per jack
// (prepared for future I2C rotary encoder input).
//
// Control Surface CCPotentiometer objects are constructed dynamically
// via placement new when an expression pedal is plugged in, and
// destroyed on unplug.  This lets Control Surface handle analog
// smoothing and filtering while still supporting runtime-configurable
// MIDI addresses.
//
// See SCHEMATIC.md for full wiring details.

#include <Control_Surface.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <new>  // placement new

// ---------------------------------------------------------------------------
// MIDI interface — Teensy native USB MIDI
// ---------------------------------------------------------------------------
USBMIDI_Interface midi;

// ---------------------------------------------------------------------------
// OLED display — SSD1306 128x64 on I2C
// ---------------------------------------------------------------------------
constexpr uint8_t OLED_WIDTH  = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr uint8_t OLED_ADDR   = 0x3C;  // typical SSD1306 address

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

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
//   TIP  — wiper on Tip, VCC/GND on Ring
//   RING — wiper on Ring, VCC/GND on Tip  (e.g. Yamaha FC3A)
//
// Set each jack to match the pedal you are plugging in.
// The non-wiper pin is used as the "sense" pin for TS/TRS detection.
// ---------------------------------------------------------------------------
enum class WiperPin : uint8_t { TIP, RING };

constexpr WiperPin J1_WIPER = WiperPin::RING;  // Yamaha FC3A: wiper on Ring
constexpr WiperPin J2_WIPER = WiperPin::TIP;   // change to RING if needed

// ---------------------------------------------------------------------------
// CC name lookup — maps CC number to a short display name
// ---------------------------------------------------------------------------
struct CCName {
  uint8_t     cc;
  const char *name;
};

const CCName CC_NAMES[] = {
  {  1, "ModWheel"},
  {  2, "Breath"  },
  {  4, "FootCtrl"},
  {  7, "Volume"  },
  { 10, "Pan"     },
  { 11, "Express" },
  { 64, "Sustain" },
  { 65, "Portamnt"},
  { 66, "Sostnuto"},
  { 67, "SoftPedl"},
  { 68, "Legato"  },
  { 69, "Hold2"   },
};
constexpr uint8_t CC_NAMES_COUNT = sizeof(CC_NAMES) / sizeof(CC_NAMES[0]);

const char *ccName(uint8_t cc) {
  for (uint8_t i = 0; i < CC_NAMES_COUNT; i++) {
    if (CC_NAMES[i].cc == cc) return CC_NAMES[i].name;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Per-jack runtime-configurable MIDI settings
//
// These are variables (not constexpr) so a future rotary encoder can
// change them at runtime.  Default values match the original design.
// ---------------------------------------------------------------------------
struct JackMidiConfig {
  uint8_t cc;       // CC number (0–127)
  uint8_t channel;  // MIDI channel (1–16, stored as 1-based for display)
};

// Defaults — edit these or change at runtime via encoder (future)
JackMidiConfig j1Midi = { 64, 5 };   // CC 64 Sustain,   Channel 5
JackMidiConfig j2Midi = { 67, 5 };   // CC 67 Soft Pedal, Channel 5
JackMidiConfig j3Midi = { 66, 5 };   // CC 66 Sostenuto,  Channel 5

// Helper: build a MIDIAddress from a JackMidiConfig
MIDIAddress midiAddr(const JackMidiConfig &cfg) {
  return {cfg.cc, Channel(cfg.channel - 1)};
}

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------
constexpr unsigned long DETECT_INTERVAL_MS   = 50;   // plug-detect poll rate
constexpr unsigned long DEBOUNCE_SETTLE_MS   = 200;  // settle time after plug event
constexpr unsigned long RING_SAMPLE_COUNT    = 8;     // samples for TS/TRS decision
constexpr uint16_t      RING_THRESHOLD       = 200;   // ADC sense-pin threshold (12-bit)
constexpr unsigned long DISPLAY_INTERVAL_MS  = 100;   // OLED refresh rate (~10 fps)

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
  unsigned long settleEnd  = 0;       // debounce after plug event
  uint8_t    lastSent      = 0xFF;    // last CC value sent (0xFF = none)
};

JackState j1State, j2State, j3State;

// ---------------------------------------------------------------------------
// Dynamically constructed Control Surface elements
//
// CCPotentiometer objects are created via placement new when an
// expression pedal is detected, and destroyed on unplug.  This lets
// Control Surface handle analog smoothing, filtering, and MIDI sends
// while allowing the MIDI address to be set at construction time from
// the current runtime config.
//
// On/off switches are handled manually because CCButton does not
// support polarity inversion (NO vs NC auto-detection).
// ---------------------------------------------------------------------------

// Aligned storage for CCPotentiometer objects
alignas(CCPotentiometer) uint8_t j1ExprBuf[sizeof(CCPotentiometer)];
alignas(CCPotentiometer) uint8_t j2ExprBuf[sizeof(CCPotentiometer)];
CCPotentiometer *j1Expr = nullptr;
CCPotentiometer *j2Expr = nullptr;

// Create a CCPotentiometer in pre-allocated storage
void createExpr(uint8_t *buf, CCPotentiometer *&ptr,
                pin_t pin, const JackMidiConfig &cfg) {
  ptr = new (buf) CCPotentiometer(pin, midiAddr(cfg));
  ptr->begin();  // init filter (Control_Surface.begin() was already called)
}

// Destroy a CCPotentiometer and clear the pointer
void destroyExpr(CCPotentiometer *&ptr) {
  if (ptr) {
    ptr->disable();
    ptr->~CCPotentiometer();
    ptr = nullptr;
  }
}

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
bool readDetect(pin_t pin);
PedalType classifyJack(pin_t sensePin, pin_t wiperPin);
PedalType classifyDigitalJack(pin_t tipPin);
void handleExpressionJack(JackState &st, pin_t detectPin, pin_t tipPin,
                          pin_t ringPin, WiperPin wiperCfg,
                          JackMidiConfig &cfg,
                          uint8_t *exprBuf, CCPotentiometer *&exprPtr);
void handleDigitalJack(JackState &st, pin_t detectPin, pin_t tipPin,
                       JackMidiConfig &cfg);
void sendCC(const JackMidiConfig &cfg, uint8_t value);
void updateDisplay();

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

  // J3 Tip — digital input with pull-up
  pinMode(J3_TIP_PIN, INPUT_PULLUP);

  // OLED init
  Wire.begin();
  if (oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println(F("Midi Expression"));
    oled.println(F("Initializing..."));
    oled.display();
  }

  Control_Surface.begin();
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop() {
  Control_Surface.loop();

  static unsigned long lastDetect  = 0;
  static unsigned long lastDisplay = 0;
  unsigned long now = millis();

  // Plug-detect and pedal management
  if (now - lastDetect >= DETECT_INTERVAL_MS) {
    lastDetect = now;

    handleExpressionJack(j1State, J1_DETECT_PIN, J1_TIP_PIN, J1_RING_PIN,
                         J1_WIPER, j1Midi, j1ExprBuf, j1Expr);
    handleExpressionJack(j2State, J2_DETECT_PIN, J2_TIP_PIN, J2_RING_PIN,
                         J2_WIPER, j2Midi, j2ExprBuf, j2Expr);
    handleDigitalJack(j3State, J3_DETECT_PIN, J3_TIP_PIN, j3Midi);
  }

  // OLED refresh
  if (now - lastDisplay >= DISPLAY_INTERVAL_MS) {
    lastDisplay = now;
    updateDisplay();
  }
}

// ---------------------------------------------------------------------------
// Plug-detect: S_s reads HIGH when a plug is inserted
// ---------------------------------------------------------------------------
bool readDetect(pin_t pin) {
  return digitalRead(pin) == HIGH;
}

// ---------------------------------------------------------------------------
// Classify an expression-capable jack as TRS or TS
// ---------------------------------------------------------------------------

// Thresholds (12-bit ADC, 0–4095)
constexpr uint16_t RAIL_LOW  = 200;   // below this → near GND rail
constexpr uint16_t RAIL_HIGH = 3895;  // above this → near VCC rail

PedalType classifyJack(pin_t sensePin, pin_t wiperPin) {
  uint32_t senseSum = 0;
  uint32_t wiperSum = 0;
  for (uint16_t i = 0; i < RING_SAMPLE_COUNT; i++) {
    senseSum += analogRead(sensePin);
    wiperSum += analogRead(wiperPin);
  }
  uint16_t senseAvg = senseSum / RING_SAMPLE_COUNT;
  uint16_t wiperAvg = wiperSum / RING_SAMPLE_COUNT;

  if (senseAvg > RING_THRESHOLD)
    return PedalType::EXPRESSION;

  bool wiperAtRail = (wiperAvg < RAIL_LOW) || (wiperAvg > RAIL_HIGH);
  if (!wiperAtRail)
    return PedalType::EXPRESSION;

  if (wiperAvg < RAIL_LOW)
    return PedalType::SWITCH_NC;
  return PedalType::SWITCH_NO;
}

// ---------------------------------------------------------------------------
// Classify J3 (digital-only) switch polarity
// ---------------------------------------------------------------------------
PedalType classifyDigitalJack(pin_t tipPin) {
  int val = digitalRead(tipPin);
  return (val == LOW) ? PedalType::SWITCH_NC : PedalType::SWITCH_NO;
}

// ---------------------------------------------------------------------------
// Send a CC message from a JackMidiConfig
// ---------------------------------------------------------------------------
void sendCC(const JackMidiConfig &cfg, uint8_t value) {
  midi.sendControlChange(midiAddr(cfg), value);
}

// ---------------------------------------------------------------------------
// Handle an expression-capable jack (J1 or J2)
//
// When a TRS expression pedal is detected, a CCPotentiometer is
// constructed via placement new so Control Surface manages smoothing,
// filtering, and MIDI output.  When the pedal is unplugged (or a TS
// switch is detected instead), the CCPotentiometer is destroyed.
//
// On/off switches are handled manually with polarity auto-detection.
// ---------------------------------------------------------------------------
void handleExpressionJack(JackState &st, pin_t detectPin, pin_t tipPin,
                          pin_t ringPin, WiperPin wiperCfg,
                          JackMidiConfig &cfg,
                          uint8_t *exprBuf, CCPotentiometer *&exprPtr) {
  pin_t wiperAnalogPin = (wiperCfg == WiperPin::TIP) ? tipPin  : ringPin;
  pin_t senseAnalogPin = (wiperCfg == WiperPin::TIP) ? ringPin : tipPin;

  unsigned long now = millis();
  bool plugged = readDetect(detectPin);

  // --- Plug event: just inserted or removed ---
  if (plugged != st.wasPlugged) {
    st.wasPlugged = plugged;
    st.settleEnd = now + DEBOUNCE_SETTLE_MS;
    st.plugged = false;
    st.type = PedalType::UNKNOWN;

    // Destroy any active CCPotentiometer
    destroyExpr(exprPtr);

    // If unplugged, send a zero to clear the controller state
    if (!plugged) {
      sendCC(cfg, 0);
      st.lastSent = 0;
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

    // If expression pedal, construct a CCPotentiometer and let
    // Control Surface take over analog reads, smoothing, and sends.
    if (st.type == PedalType::EXPRESSION) {
      createExpr(exprBuf, exprPtr, wiperAnalogPin, cfg);
    }
  }

  if (!st.plugged)
    return;

  // --- Expression pedal (TRS) — Control Surface handles everything ---
  if (st.type == PedalType::EXPRESSION) {
    // Read the wiper pin for display purposes only.
    // Control_Surface.loop() handles the actual filtered MIDI sends.
    uint8_t approx = analogRead(wiperAnalogPin) >> 5;
    if (approx > 127) approx = 127;
    st.lastSent = approx;
    return;
  }

  // --- On/off switch (TS) — manual send with polarity handling ---
  int raw = digitalRead(wiperAnalogPin);

  bool pressed;
  if (st.type == PedalType::SWITCH_NC)
    pressed = (raw == HIGH);
  else
    pressed = (raw == LOW);

  uint8_t value = pressed ? 0x7F : 0x00;
  if (value != st.lastSent) {
    sendCC(cfg, value);
    st.lastSent = value;
  }
}

// ---------------------------------------------------------------------------
// Handle the digital-only jack (J3)
// ---------------------------------------------------------------------------
void handleDigitalJack(JackState &st, pin_t detectPin, pin_t tipPin,
                       JackMidiConfig &cfg) {
  unsigned long now = millis();
  bool plugged = readDetect(detectPin);

  // --- Plug event ---
  if (plugged != st.wasPlugged) {
    st.wasPlugged = plugged;
    st.settleEnd = now + DEBOUNCE_SETTLE_MS;
    st.plugged = false;
    st.type = PedalType::UNKNOWN;

    if (!plugged) {
      sendCC(cfg, 0);
      st.lastSent = 0;
    }
    return;
  }

  if (st.settleEnd != 0 && now < st.settleEnd)
    return;

  if (st.settleEnd != 0 && now >= st.settleEnd) {
    st.settleEnd = 0;
    if (!plugged) {
      st.plugged = false;
      return;
    }
    st.plugged = true;
    pinMode(tipPin, INPUT_PULLUP);
    delay(5);
    st.type = classifyDigitalJack(tipPin);
  }

  if (!st.plugged)
    return;

  int raw = digitalRead(tipPin);

  bool pressed;
  if (st.type == PedalType::SWITCH_NC)
    pressed = (raw == HIGH);
  else
    pressed = (raw == LOW);

  uint8_t value = pressed ? 0x7F : 0x00;
  if (value != st.lastSent) {
    sendCC(cfg, value);
    st.lastSent = value;
  }
}

// ---------------------------------------------------------------------------
// OLED display — draw live status for all three jacks
//
// Layout (128x64, 6x8 font = 21 chars x 8 rows):
//
//   Row 0:  "Midi Expression"           (header)
//   Row 1:  ────────────────────         (separator)
//   Row 2:  J1: Plugged  TRS            (status)
//   Row 3:      CC64 Sustain  ch5  127  (midi info + value)
//   Row 4:  J2: Unplugged               (status)
//   Row 5:      CC67 SoftPedl ch5  ---  (midi info)
//   Row 6:  J3: Plugged  TS             (status)
//   Row 7:      CC66 Sostnuto ch5  127  (midi info + value)
// ---------------------------------------------------------------------------

void drawJackRow(uint8_t y, uint8_t jackNum, const JackState &st,
                 const JackMidiConfig &cfg) {
  // Line 1: "Jn: In TRS Expr" or "Jn: Unplugged"
  oled.setCursor(0, y);
  oled.print(F("J"));
  oled.print(jackNum);
  oled.print(F(":"));

  if (!st.plugged) {
    oled.print(F(" Unplugged"));
  } else {
    oled.print(F(" In "));
    switch (st.type) {
      case PedalType::EXPRESSION: oled.print(F("TRS Expr")); break;
      case PedalType::SWITCH_NO:  oled.print(F("TS  NO"));   break;
      case PedalType::SWITCH_NC:  oled.print(F("TS  NC"));   break;
      default:                    oled.print(F("..."));       break;
    }
  }

  // Line 2: "  CC 64 Sustain  c 5  127"
  oled.setCursor(0, y + 8);
  oled.print(F("  CC"));
  if (cfg.cc < 100) oled.print(F(" "));
  if (cfg.cc < 10)  oled.print(F(" "));
  oled.print(cfg.cc);
  oled.print(F(" "));

  const char *name = ccName(cfg.cc);
  if (name) {
    oled.print(name);
  } else {
    oled.print(F("CC"));
    oled.print(cfg.cc);
  }

  // Channel at column 84, value at column 108
  oled.setCursor(84, y + 8);
  oled.print(F("c"));
  if (cfg.channel < 10) oled.print(F(" "));
  oled.print(cfg.channel);

  oled.setCursor(108, y + 8);
  if (!st.plugged || st.lastSent == 0xFF) {
    oled.print(F("---"));
  } else {
    if (st.lastSent < 100) oled.print(F(" "));
    if (st.lastSent < 10)  oled.print(F(" "));
    oled.print(st.lastSent);
  }
}

void updateDisplay() {
  oled.clearDisplay();

  // Header
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(F("Midi Expression"));

  // Separator — dotted line at y=9
  for (uint8_t x = 0; x < OLED_WIDTH; x += 3) {
    oled.drawPixel(x, 9, SSD1306_WHITE);
  }

  // Three jack rows, each 16px tall (2 text lines of 8px)
  drawJackRow(12, 1, j1State, j1Midi);
  drawJackRow(30, 2, j2State, j2Midi);
  drawJackRow(48, 3, j3State, j3Midi);

  oled.display();
}
