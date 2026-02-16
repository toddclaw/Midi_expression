// Midi Expression — USB MIDI pedal controller
// Teensy 4.1 (USB Type: "MIDI" in Arduino IDE / Tools menu)
// Control Surface library by tttapa
//
// Four 6-pin switched 1/4" TRS sockets (all expression-capable):
//   J1 — Sustain   (CC 64)  Channel 5
//   J2 — Soft      (CC 67)  Channel 5
//   J3 — Sostenuto (CC 66)  Channel 5
//   J4 — Pedal 4   (CC 11)  Channel 5
//
// 128x64 SSD1306 OLED on Wire2 I2C (SDA2=25, SCL2=24) displays live status.
// MIDI CC number and channel are runtime-configurable per jack
// via a KY-040 rotary encoder (CLK=pin 4, DT=pin 5, SW=pin 6).
//
// Control Surface objects (CCPotentiometer, CCButton) are constructed
// dynamically via placement new when a pedal is plugged in, and
// destroyed on unplug.  CCButton.invert() handles NC switch polarity.
//
// See SCHEMATIC.md for full wiring details.

#include <Control_Surface.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <EEPROM.h>
#include <new>  // placement new

// ---------------------------------------------------------------------------
// MIDI interface — Teensy native USB MIDI
// ---------------------------------------------------------------------------
USBMIDI_Interface midi;

// ---------------------------------------------------------------------------
// OLED display — SSD1306 128x64 on I2C (Wire2: SCL2=24, SDA2=25)
// ---------------------------------------------------------------------------
constexpr uint8_t OLED_WIDTH  = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr uint8_t OLED_ADDR   = 0x3C;  // typical SSD1306 address

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire2, -1);

// ---------------------------------------------------------------------------
// Pin definitions (from SCHEMATIC.md)
// ---------------------------------------------------------------------------

// J1 — expression-capable socket (Sustain / Yamaha FC3A)
constexpr pin_t J1_TIP_PIN    = A0;  // pin 14 — analog, pedal wiper
constexpr pin_t J1_RING_PIN   = A1;  // pin 15 — analog, VCC sense
constexpr pin_t J1_DETECT_PIN = 0;   // pin 0  — S_s plug detect

// J2 — expression-capable socket (Soft)
constexpr pin_t J2_TIP_PIN    = A2;  // pin 16 — analog, pedal wiper
constexpr pin_t J2_RING_PIN   = A3;  // pin 17 — analog, VCC sense
constexpr pin_t J2_DETECT_PIN = 1;   // pin 1  — S_s plug detect

// J3 — expression-capable socket (Sostenuto)
constexpr pin_t J3_TIP_PIN    = A4;  // pin 18 — analog, pedal wiper
constexpr pin_t J3_RING_PIN   = A5;  // pin 19 — analog, VCC sense
constexpr pin_t J3_DETECT_PIN = 2;   // pin 2  — S_s plug detect

// J4 — expression-capable socket (Pedal 4)
constexpr pin_t J4_TIP_PIN    = A6;  // pin 20 — analog, pedal wiper
constexpr pin_t J4_RING_PIN   = A7;  // pin 21 — analog, VCC sense
constexpr pin_t J4_DETECT_PIN = 3;   // pin 3  — S_s plug detect

// KY-040 rotary encoder (active-LOW outputs, power from 3.3 V only)
constexpr pin_t ENC_CLK_PIN = 4;   // encoder A phase
constexpr pin_t ENC_DT_PIN  = 5;   // encoder B phase
constexpr pin_t ENC_SW_PIN  = 6;   // push-button

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
constexpr WiperPin J3_WIPER = WiperPin::TIP;   // change to RING if needed
constexpr WiperPin J4_WIPER = WiperPin::TIP;   // change to RING if needed

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
// These are variables (not constexpr) so the rotary encoder can
// change them at runtime.  Default values match the original design.
// ---------------------------------------------------------------------------
struct JackMidiConfig {
  uint8_t  cc;       // CC number (0–127)
  uint8_t  channel;  // MIDI channel (1–16, stored as 1-based for display)
  bool     inverted; // Invert expression pedal output (127→0, 0→127)
  uint16_t calMin;   // Calibrated minimum ADC value (12-bit: 0–4095)
  uint16_t calMax;   // Calibrated maximum ADC value (12-bit: 0–4095)
};

// Defaults — edit these or change at runtime via the rotary encoder
// Default calibration range is full ADC span (0–4095)
JackMidiConfig j1Midi = { 64, 5, false, 0, 4095 };   // CC 64 Sustain,     Channel 5
JackMidiConfig j2Midi = { 67, 5, false, 0, 4095 };   // CC 67 Soft Pedal,  Channel 5
JackMidiConfig j3Midi = { 66, 5, false, 0, 4095 };   // CC 66 Sostenuto,   Channel 5
JackMidiConfig j4Midi = { 11, 5, false, 0, 4095 };   // CC 11 Expression,  Channel 5

// Helper: build a MIDIAddress from a JackMidiConfig
MIDIAddress midiAddr(const JackMidiConfig &cfg) {
  return {cfg.cc, Channel(cfg.channel - 1)};
}

// ---------------------------------------------------------------------------
// EEPROM persistence — save/load configuration
// ---------------------------------------------------------------------------
constexpr uint16_t EEPROM_MAGIC = 0xABCD;  // sentinel for valid config
constexpr uint16_t EEPROM_ADDR  = 0;       // start address in EEPROM

struct EEPROMConfig {
  uint16_t       magic;
  JackMidiConfig j1;
  JackMidiConfig j2;
  JackMidiConfig j3;
  JackMidiConfig j4;
};

void saveConfig() {
  EEPROMConfig cfg;
  cfg.magic = EEPROM_MAGIC;
  cfg.j1 = j1Midi;
  cfg.j2 = j2Midi;
  cfg.j3 = j3Midi;
  cfg.j4 = j4Midi;
  EEPROM.put(EEPROM_ADDR, cfg);
}

void loadConfig() {
  EEPROMConfig cfg;
  EEPROM.get(EEPROM_ADDR, cfg);
  if (cfg.magic == EEPROM_MAGIC) {
    j1Midi = cfg.j1;
    j2Midi = cfg.j2;
    j3Midi = cfg.j3;
    j4Midi = cfg.j4;
  }
  // If magic doesn't match, keep defaults
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
// Rotary encoder — KY-040 for live MIDI parameter editing
//
// Push the button to cycle through editable fields:
//   (off) → J1 CC → J1 Ch → J1 Inv → J1 Cal → J2 CC → J2 Ch → J2 Inv → J2 Cal →
//           J3 CC → J3 Ch → J3 Inv → J3 Cal → J4 CC → J4 Ch → J4 Inv → J4 Cal → (off)
// Rotate to change the selected value (or toggle for Inv/Cal).
// CAL mode: Rotate encoder to enter/exit calibration. While calibrating,
// exercise the pedal to full range. Exit CAL to save min/max to EEPROM.
// Edit mode auto-exits after 10 seconds of inactivity.
// ---------------------------------------------------------------------------
enum class EditField : uint8_t {
  NONE,
  J1_CC, J1_CH, J1_INV, J1_CAL,
  J2_CC, J2_CH, J2_INV, J2_CAL,
  J3_CC, J3_CH, J3_INV, J3_CAL,
  J4_CC, J4_CH, J4_INV, J4_CAL,
};
constexpr uint8_t EDIT_FIELD_COUNT = 16;

EditField     editField        = EditField::NONE;
unsigned long editLastActivity = 0;
constexpr unsigned long EDIT_TIMEOUT_MS = 10000;  // auto-exit after 10 s

// Button debounce
bool          encBtnLast       = HIGH;
unsigned long encBtnDebounce   = 0;
constexpr unsigned long ENC_BTN_DEBOUNCE_MS = 50;

// Encoder (PJRC Encoder library — interrupt-capable on Teensy 4.x)
Encoder  enc(ENC_CLK_PIN, ENC_DT_PIN);
long     encLastPos  = 0;
constexpr int ENC_COUNTS_PER_DETENT = 4;  // PJRC library gives 4 counts per detent

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
  bool       calibrating   = false;   // true when in calibration mode
  uint16_t   calMinSeen    = 4095;    // min ADC value seen during cal
  uint16_t   calMaxSeen    = 0;       // max ADC value seen during cal
};

JackState j1State, j2State, j3State, j4State;

// ---------------------------------------------------------------------------
// Dynamically constructed Control Surface elements
//
// CCPotentiometer and CCButton objects are created via placement new
// when a pedal is detected, and destroyed on unplug.  This lets
// Control Surface handle smoothing/filtering (expression) and
// debouncing (switches).  CCButton.invert() handles NC polarity.
// ---------------------------------------------------------------------------

// Aligned storage
alignas(CCPotentiometer) uint8_t j1ExprBuf[sizeof(CCPotentiometer)];
alignas(CCPotentiometer) uint8_t j2ExprBuf[sizeof(CCPotentiometer)];
alignas(CCPotentiometer) uint8_t j3ExprBuf[sizeof(CCPotentiometer)];
alignas(CCPotentiometer) uint8_t j4ExprBuf[sizeof(CCPotentiometer)];
alignas(CCButton) uint8_t j1BtnBuf[sizeof(CCButton)];
alignas(CCButton) uint8_t j2BtnBuf[sizeof(CCButton)];
alignas(CCButton) uint8_t j3BtnBuf[sizeof(CCButton)];
alignas(CCButton) uint8_t j4BtnBuf[sizeof(CCButton)];
CCPotentiometer *j1Expr = nullptr;
CCPotentiometer *j2Expr = nullptr;
CCPotentiometer *j3Expr = nullptr;
CCPotentiometer *j4Expr = nullptr;
CCButton *j1Btn = nullptr;
CCButton *j2Btn = nullptr;
CCButton *j3Btn = nullptr;
CCButton *j4Btn = nullptr;

void createExpr(uint8_t *buf, CCPotentiometer *&ptr,
                pin_t pin, const JackMidiConfig &cfg) {
  ptr = new (buf) CCPotentiometer(pin, midiAddr(cfg));
  ptr->begin();
  if (cfg.inverted) ptr->invert();
}

void destroyExpr(CCPotentiometer *&ptr) {
  if (ptr) { ptr->disable(); ptr->~CCPotentiometer(); ptr = nullptr; }
}

void createBtn(uint8_t *buf, CCButton *&ptr,
               pin_t pin, const JackMidiConfig &cfg, bool nc) {
  ptr = new (buf) CCButton(pin, midiAddr(cfg));
  ptr->begin();
  if (nc) ptr->invert();
}

void destroyBtn(CCButton *&ptr) {
  if (ptr) { ptr->disable(); ptr->~CCButton(); ptr = nullptr; }
}

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void handleExpressionJack(JackState &st, pin_t detectPin, pin_t tipPin,
                          pin_t ringPin, WiperPin wiperCfg,
                          JackMidiConfig &cfg,
                          uint8_t *exprBuf, CCPotentiometer *&exprPtr,
                          uint8_t *btnBuf, CCButton *&btnPtr);
void handleEncoder();
void applyEncoderChange(int steps);
void rebuildExprJack(JackState &st, pin_t tipPin, pin_t ringPin,
                     WiperPin wiperCfg, JackMidiConfig &cfg,
                     uint8_t *exprBuf, CCPotentiometer *&exprPtr,
                     uint8_t *btnBuf, CCButton *&btnPtr);
void updateDisplay();

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
  // 12-bit ADC for finer expression control
  analogReadResolution(12);

  // Load saved configuration from EEPROM
  loadConfig();

  // Plug-detect pins — INPUT_PULLUP (S_s pins)
  pinMode(J1_DETECT_PIN, INPUT_PULLUP);
  pinMode(J2_DETECT_PIN, INPUT_PULLUP);
  pinMode(J3_DETECT_PIN, INPUT_PULLUP);
  pinMode(J4_DETECT_PIN, INPUT_PULLUP);

  // Encoder button pin (CLK/DT handled by Encoder library)
  pinMode(ENC_SW_PIN,  INPUT_PULLUP);

  // OLED init (Wire2: SCL2=24, SDA2=25)
  Wire2.begin();
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

  // Rotary encoder input (every loop — encoder is interrupt-driven)
  handleEncoder();

  // Plug-detect and pedal management
  if (now - lastDetect >= DETECT_INTERVAL_MS) {
    lastDetect = now;

    handleExpressionJack(j1State, J1_DETECT_PIN, J1_TIP_PIN, J1_RING_PIN,
                         J1_WIPER, j1Midi, j1ExprBuf, j1Expr,
                         j1BtnBuf, j1Btn);
    handleExpressionJack(j2State, J2_DETECT_PIN, J2_TIP_PIN, J2_RING_PIN,
                         J2_WIPER, j2Midi, j2ExprBuf, j2Expr,
                         j2BtnBuf, j2Btn);
    handleExpressionJack(j3State, J3_DETECT_PIN, J3_TIP_PIN, J3_RING_PIN,
                         J3_WIPER, j3Midi, j3ExprBuf, j3Expr,
                         j3BtnBuf, j3Btn);
    handleExpressionJack(j4State, J4_DETECT_PIN, J4_TIP_PIN, J4_RING_PIN,
                         J4_WIPER, j4Midi, j4ExprBuf, j4Expr,
                         j4BtnBuf, j4Btn);
  }

  // OLED refresh
  if (now - lastDisplay >= DISPLAY_INTERVAL_MS) {
    lastDisplay = now;
    updateDisplay();
  }
}

// ---------------------------------------------------------------------------
// Classification helpers
// ---------------------------------------------------------------------------

// Thresholds (12-bit ADC, 0–4095)
constexpr uint16_t RAIL_LOW  = 200;   // below this → near GND rail
constexpr uint16_t RAIL_HIGH = 3895;  // above this → near VCC rail

PedalType classifyJack(pin_t sensePin, pin_t wiperPin) {
  uint32_t senseSum = 0, wiperSum = 0;
  for (uint16_t i = 0; i < RING_SAMPLE_COUNT; i++) {
    senseSum += analogRead(sensePin);
    wiperSum += analogRead(wiperPin);
  }
  uint16_t senseAvg = senseSum / RING_SAMPLE_COUNT;
  uint16_t wiperAvg = wiperSum / RING_SAMPLE_COUNT;

  if (senseAvg > RING_THRESHOLD) return PedalType::EXPRESSION;
  if (wiperAvg > RAIL_LOW && wiperAvg < RAIL_HIGH) return PedalType::EXPRESSION;
  return (wiperAvg < RAIL_LOW) ? PedalType::SWITCH_NC : PedalType::SWITCH_NO;
}

// ---------------------------------------------------------------------------
// Handle an expression-capable jack (J1 or J2)
//
// TRS expression → CCPotentiometer (Control Surface smoothing + sends)
// TS switch      → CCButton + invert() for NC (Control Surface debouncing)
// ---------------------------------------------------------------------------
void handleExpressionJack(JackState &st, pin_t detectPin, pin_t tipPin,
                          pin_t ringPin, WiperPin wiperCfg,
                          JackMidiConfig &cfg,
                          uint8_t *exprBuf, CCPotentiometer *&exprPtr,
                          uint8_t *btnBuf, CCButton *&btnPtr) {
  pin_t wiperPin = (wiperCfg == WiperPin::TIP) ? tipPin  : ringPin;
  pin_t sensePin = (wiperCfg == WiperPin::TIP) ? ringPin : tipPin;
  unsigned long now = millis();
  bool plugged = (digitalRead(detectPin) == HIGH);

  // Plug event
  if (plugged != st.wasPlugged) {
    st.wasPlugged = plugged;
    st.settleEnd = now + DEBOUNCE_SETTLE_MS;
    st.plugged = false;
    st.type = PedalType::UNKNOWN;
    destroyExpr(exprPtr);
    destroyBtn(btnPtr);
    if (!plugged) {
      midi.sendCC(midiAddr(cfg), 0);
      st.lastSent = 0;
    }
    return;
  }

  // Settling
  if (st.settleEnd != 0 && now < st.settleEnd) return;

  // Settle complete → classify and construct
  if (st.settleEnd != 0) {
    st.settleEnd = 0;
    if (!plugged) { st.plugged = false; return; }
    st.plugged = true;
    st.type = classifyJack(sensePin, wiperPin);
    if (st.type == PedalType::EXPRESSION)
      createExpr(exprBuf, exprPtr, wiperPin, cfg);
    else
      createBtn(btnBuf, btnPtr, wiperPin, cfg, st.type == PedalType::SWITCH_NC);
  }

  if (!st.plugged) return;

  // Update lastSent for display (Control Surface handles actual MIDI)
  if (st.type == PedalType::EXPRESSION) {
    uint16_t raw = analogRead(wiperPin);

    // If calibrating, track min/max
    if (st.calibrating) {
      if (raw < st.calMinSeen) st.calMinSeen = raw;
      if (raw > st.calMaxSeen) st.calMaxSeen = raw;
    }

    // Apply calibration mapping: raw [calMin..calMax] → [0..127]
    uint16_t calMin = cfg.calMin;
    uint16_t calMax = cfg.calMax;
    if (calMax <= calMin) {
      // Invalid calibration — fallback to raw >> 5
      st.lastSent = min((uint8_t)(raw >> 5), (uint8_t)127);
    } else {
      // Map calibrated range to 0-127
      int32_t mapped = ((int32_t)raw - calMin) * 127 / (calMax - calMin);
      st.lastSent = constrain(mapped, 0, 127);
    }
  } else {
    bool pressed = (st.type == PedalType::SWITCH_NC)
                   ? (digitalRead(wiperPin) == HIGH)
                   : (digitalRead(wiperPin) == LOW);
    st.lastSent = pressed ? 0x7F : 0x00;
  }
}

// ---------------------------------------------------------------------------
// Rotary encoder — rebuild helpers & input handling
// ---------------------------------------------------------------------------

void rebuildExprJack(JackState &st, pin_t tipPin, pin_t ringPin,
                     WiperPin wiperCfg, JackMidiConfig &cfg,
                     uint8_t *exprBuf, CCPotentiometer *&exprPtr,
                     uint8_t *btnBuf, CCButton *&btnPtr) {
  if (!st.plugged) return;
  pin_t wiperPin = (wiperCfg == WiperPin::TIP) ? tipPin : ringPin;
  if (st.type == PedalType::EXPRESSION) {
    destroyExpr(exprPtr);
    createExpr(exprBuf, exprPtr, wiperPin, cfg);
  } else if (st.type == PedalType::SWITCH_NO || st.type == PedalType::SWITCH_NC) {
    destroyBtn(btnPtr);
    createBtn(btnBuf, btnPtr, wiperPin, cfg, st.type == PedalType::SWITCH_NC);
  }
}

void applyEncoderChange(int steps) {
  JackMidiConfig *cfg = nullptr;
  JackState *st = nullptr;
  enum FieldType { CC, CH, INV, CAL } fieldType;

  switch (editField) {
    case EditField::J1_CC:  cfg = &j1Midi; st = &j1State; fieldType = CC;  break;
    case EditField::J1_CH:  cfg = &j1Midi; st = &j1State; fieldType = CH;  break;
    case EditField::J1_INV: cfg = &j1Midi; st = &j1State; fieldType = INV; break;
    case EditField::J1_CAL: cfg = &j1Midi; st = &j1State; fieldType = CAL; break;
    case EditField::J2_CC:  cfg = &j2Midi; st = &j2State; fieldType = CC;  break;
    case EditField::J2_CH:  cfg = &j2Midi; st = &j2State; fieldType = CH;  break;
    case EditField::J2_INV: cfg = &j2Midi; st = &j2State; fieldType = INV; break;
    case EditField::J2_CAL: cfg = &j2Midi; st = &j2State; fieldType = CAL; break;
    case EditField::J3_CC:  cfg = &j3Midi; st = &j3State; fieldType = CC;  break;
    case EditField::J3_CH:  cfg = &j3Midi; st = &j3State; fieldType = CH;  break;
    case EditField::J3_INV: cfg = &j3Midi; st = &j3State; fieldType = INV; break;
    case EditField::J3_CAL: cfg = &j3Midi; st = &j3State; fieldType = CAL; break;
    case EditField::J4_CC:  cfg = &j4Midi; st = &j4State; fieldType = CC;  break;
    case EditField::J4_CH:  cfg = &j4Midi; st = &j4State; fieldType = CH;  break;
    case EditField::J4_INV: cfg = &j4Midi; st = &j4State; fieldType = INV; break;
    case EditField::J4_CAL: cfg = &j4Midi; st = &j4State; fieldType = CAL; break;
    default: return;
  }

  uint8_t  oldCC  = cfg->cc;
  uint8_t  oldCh  = cfg->channel;
  bool     oldInv = cfg->inverted;
  uint16_t oldMin = cfg->calMin;
  uint16_t oldMax = cfg->calMax;

  if (fieldType == CC) {
    int v = (int)cfg->cc + steps;
    cfg->cc = constrain(v, 0, 127);
  } else if (fieldType == CH) {
    int v = (int)cfg->channel + steps;
    cfg->channel = constrain(v, 1, 16);
  } else if (fieldType == INV) {
    cfg->inverted = !cfg->inverted;
  } else if (fieldType == CAL) {
    // Toggle calibration mode on any rotation
    if (!st->calibrating) {
      // Enter calibration mode — reset min/max tracking
      st->calibrating = true;
      st->calMinSeen = 4095;
      st->calMaxSeen = 0;
    } else {
      // Exit calibration mode — save captured range to config
      st->calibrating = false;
      cfg->calMin = st->calMinSeen;
      cfg->calMax = st->calMaxSeen;
    }
  }

  // Save to EEPROM whenever a change is made (except during active calibration)
  bool configChanged = (cfg->cc != oldCC || cfg->channel != oldCh ||
                        cfg->inverted != oldInv || cfg->calMin != oldMin ||
                        cfg->calMax != oldMax);
  if (configChanged && !st->calibrating) {
    saveConfig();
  }

  // No rebuild needed if nothing changed or just toggled into CAL mode
  if (!configChanged) return;

  // Zero the old CC so the host doesn't see a stuck controller
  if (cfg->cc != oldCC || cfg->channel != oldCh) {
    midi.sendCC({oldCC, Channel(oldCh - 1)}, 0);
  }

  // Rebuild the active Control Surface object with the new settings
  // (skipping during calibration since we're just tracking min/max)
  if (!st->calibrating) {
    switch (editField) {
      case EditField::J1_CC:
      case EditField::J1_CH:
      case EditField::J1_INV:
      case EditField::J1_CAL:
        rebuildExprJack(j1State, J1_TIP_PIN, J1_RING_PIN, J1_WIPER,
                        j1Midi, j1ExprBuf, j1Expr, j1BtnBuf, j1Btn);
        break;
      case EditField::J2_CC:
      case EditField::J2_CH:
      case EditField::J2_INV:
      case EditField::J2_CAL:
        rebuildExprJack(j2State, J2_TIP_PIN, J2_RING_PIN, J2_WIPER,
                        j2Midi, j2ExprBuf, j2Expr, j2BtnBuf, j2Btn);
        break;
      case EditField::J3_CC:
      case EditField::J3_CH:
      case EditField::J3_INV:
      case EditField::J3_CAL:
        rebuildExprJack(j3State, J3_TIP_PIN, J3_RING_PIN, J3_WIPER,
                        j3Midi, j3ExprBuf, j3Expr, j3BtnBuf, j3Btn);
        break;
      case EditField::J4_CC:
      case EditField::J4_CH:
      case EditField::J4_INV:
      case EditField::J4_CAL:
        rebuildExprJack(j4State, J4_TIP_PIN, J4_RING_PIN, J4_WIPER,
                        j4Midi, j4ExprBuf, j4Expr, j4BtnBuf, j4Btn);
        break;
      default: break;
    }
  }
}

void handleEncoder() {
  unsigned long now = millis();

  // Read encoder position (PJRC library tracks via interrupts)
  long encPosition = enc.read();

  // --- Button: cycle through editable fields ---
  bool btn = digitalRead(ENC_SW_PIN);
  if (btn != encBtnLast && (now - encBtnDebounce) >= ENC_BTN_DEBOUNCE_MS) {
    encBtnDebounce = now;
    encBtnLast = btn;
    if (btn == LOW) {  // active-low (KY-040 pulls to GND when pressed)
      editLastActivity = now;
      uint8_t f = static_cast<uint8_t>(editField) + 1;
      if (f > EDIT_FIELD_COUNT) f = 0;
      editField = static_cast<EditField>(f);
      encLastPos = encPosition;  // reset baseline on field change
    }
  }

  // --- Auto-timeout ---
  if (editField != EditField::NONE &&
      (now - editLastActivity) >= EDIT_TIMEOUT_MS) {
    editField = EditField::NONE;
    return;
  }

  if (editField == EditField::NONE) return;

  // --- Rotation: change selected value ---
  long diff = encPosition - encLastPos;
  if (abs(diff) < ENC_COUNTS_PER_DETENT) return;

  int steps = diff / ENC_COUNTS_PER_DETENT;
  encLastPos += steps * ENC_COUNTS_PER_DETENT;
  editLastActivity = now;

  applyEncoderChange(steps);
}

// ---------------------------------------------------------------------------
// OLED display — draw live status for all four jacks
//
// Layout (128x64, 6x8 font = 21 chars x 8 rows, compact 14-pixel spacing):
//
//   Row 0:  "Midi Expression"           (header, y=0-7)
//   Jack 1:  J1: In TRS Expr            (y=8-15, status)
//            CC64 Sustain  ch5  127     (y=16-21, midi info + value)
//   Jack 2:  J2: Unplugged              (y=22-29, status)
//            CC67 SoftPedl ch5  ---     (y=30-35, midi info)
//   Jack 3:  J3: In TRS Expr            (y=36-43, status)
//            CC66 Sostnuto ch5  127     (y=44-49, midi info + value)
//   Jack 4:  J4: In TRS Expr            (y=50-57, status)
//            CC11 Express  ch5  127     (y=58-63, midi info + value)
// ---------------------------------------------------------------------------

void drawJackRow(uint8_t y, uint8_t jackNum, const JackState &st,
                 const JackMidiConfig &cfg, bool hlCC, bool hlCh, bool hlInv, bool hlCal) {
  // Line 1: "Jn: In TRS Expr" or "Jn: Unplugged" + optional "I" for inverted
  oled.setCursor(0, y);
  oled.print(F("J"));
  oled.print(jackNum);
  oled.print(F(":"));

  if (!st.plugged) {
    oled.print(F(" Unplugged"));
  } else if (st.calibrating) {
    // Show "CAL" mode when actively calibrating
    oled.print(F(" CAL "));
    if (hlCal) oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    oled.print(st.calMinSeen);
    oled.print(F("-"));
    oled.print(st.calMaxSeen);
    if (hlCal) oled.setTextColor(SSD1306_WHITE);
  } else {
    oled.print(F(" In "));
    switch (st.type) {
      case PedalType::EXPRESSION: oled.print(F("TRS Expr")); break;
      case PedalType::SWITCH_NO:  oled.print(F("TS  NO"));   break;
      case PedalType::SWITCH_NC:  oled.print(F("TS  NC"));   break;
      default:                    oled.print(F("..."));       break;
    }
  }

  // Show "I" indicator at end of line if inverted (unless calibrating)
  if (!st.calibrating && cfg.inverted && st.type == PedalType::EXPRESSION) {
    oled.setCursor(114, y);
    if (hlInv) oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    oled.print(F("I"));
    if (hlInv) oled.setTextColor(SSD1306_WHITE);
  }

  // Line 2: "  CC 64 Sustain  c 5  127"
  oled.setCursor(0, y + 8);
  if (hlCC) oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
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
  if (hlCC) oled.setTextColor(SSD1306_WHITE);

  // Channel at column 84, value at column 108
  oled.setCursor(84, y + 8);
  if (hlCh) oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  oled.print(F("c"));
  if (cfg.channel < 10) oled.print(F(" "));
  oled.print(cfg.channel);
  if (hlCh) oled.setTextColor(SSD1306_WHITE);

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

  // Show "EDIT" indicator in top-right when editing
  if (editField != EditField::NONE) {
    oled.setCursor(102, 0);
    oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    oled.print(F("EDIT"));
    oled.setTextColor(SSD1306_WHITE);
  }

  // Four jack rows, each 14px tall (2 text lines with compact spacing)
  drawJackRow(8, 1, j1State, j1Midi,
              editField == EditField::J1_CC, editField == EditField::J1_CH,
              editField == EditField::J1_INV, editField == EditField::J1_CAL);
  drawJackRow(22, 2, j2State, j2Midi,
              editField == EditField::J2_CC, editField == EditField::J2_CH,
              editField == EditField::J2_INV, editField == EditField::J2_CAL);
  drawJackRow(36, 3, j3State, j3Midi,
              editField == EditField::J3_CC, editField == EditField::J3_CH,
              editField == EditField::J3_INV, editField == EditField::J3_CAL);
  drawJackRow(50, 4, j4State, j4Midi,
              editField == EditField::J4_CC, editField == EditField::J4_CH,
              editField == EditField::J4_INV, editField == EditField::J4_CAL);

  oled.display();
}
