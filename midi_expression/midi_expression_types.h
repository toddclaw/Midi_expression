// midi_expression_types.h
//
// Struct and enum definitions extracted into a header so that Arduino IDE's
// auto-generated function prototypes (inserted after #includes but before the
// sketch body) can reference these types without "does not name a type" errors.

#pragma once

// ---------------------------------------------------------------------------
// Per-jack runtime-configurable MIDI settings
//
// These are variables (not constexpr) so the rotary encoder can
// change them at runtime.  Default values match the original design.
// ---------------------------------------------------------------------------
struct JackMidiConfig {
  uint8_t  cc;          // CC number (0–127)
  uint8_t  channel;     // MIDI channel (1–16, stored as 1-based for display)
  bool     inverted;    // Invert expression pedal output (127→0, 0→127)
  bool     wiperOnRing; // true = wiper on Ring (e.g. Yamaha FC3A), false = wiper on Tip
  uint16_t calMin;      // Calibrated minimum ADC value (12-bit: 0–4095)
  uint16_t calMax;      // Calibrated maximum ADC value (12-bit: 0–4095)
};

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
  bool          plugged      = false;
  bool          wasPlugged   = false;
  PedalType     type         = PedalType::UNKNOWN;
  unsigned long settleEnd    = 0;      // debounce after plug event
  uint8_t       lastSent     = 0xFF;   // last CC value sent (0xFF = none)
  bool          calibrating  = false;  // true when in calibration mode
  uint16_t      calMinSeen   = 4095;   // min ADC value seen during cal
  uint16_t      calMaxSeen   = 0;      // max ADC value seen during cal
};

// ---------------------------------------------------------------------------
// Rotary encoder editable field enumeration
//
// Push the button to cycle through editable fields:
//   (off) → J1 CC → J1 Ch → J1 Inv → J1 Wip → J1 Cal →
//           J2 CC → J2 Ch → J2 Inv → J2 Wip → J2 Cal →
//           J3 CC → J3 Ch → J3 Inv → J3 Wip → J3 Cal →
//           J4 CC → J4 Ch → J4 Inv → J4 Wip → J4 Cal → (off)
// Rotate to change the selected value (or toggle for Inv/Wip/Cal).
// CAL mode: Rotate encoder to enter/exit calibration. While calibrating,
// exercise the pedal to full range. Exit CAL to save min/max to EEPROM.
// Edit mode auto-exits after 10 seconds of inactivity.
// ---------------------------------------------------------------------------
enum class EditField : uint8_t {
  NONE,
  J1_CC, J1_CH, J1_INV, J1_WIP, J1_CAL,
  J2_CC, J2_CH, J2_INV, J2_WIP, J2_CAL,
  J3_CC, J3_CH, J3_INV, J3_WIP, J3_CAL,
  J4_CC, J4_CH, J4_INV, J4_WIP, J4_CAL,
};
constexpr uint8_t EDIT_FIELD_COUNT = 20;
