# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Teensy 4.1 firmware for a USB MIDI pedal controller with four expression-capable 1/4" TRS jacks. The device enumerates as a class-compliant USB MIDI device and auto-detects plugged pedals as TRS expression pedals or TS on/off switches (NO/NC). MIDI CC number, channel, invert, wiper polarity, and calibration are configurable per jack via a KY-040 rotary encoder and shown on an SSD1306 OLED display.

## Building and Flashing

**Toolchain:** Arduino IDE + Teensyduino (PJRC). There is no Makefile or CLI build — use the Arduino IDE GUI.

**Critical IDE settings (Tools menu):**
- Board: `Teensy 4.1`
- USB Type: **MIDI** (required — the sketch uses `USBMIDI_Interface`)

**Required Arduino libraries (install via Library Manager):**
- `Control Surface` by tttapa
- `Adafruit SSD1306` + `Adafruit GFX Library`
- `Encoder` by Paul Stoffregen (PJRC)

**Sketch to upload:** `midi_expression/midi_expression.ino` (the `midi_expression/` subdirectory is the canonical Arduino sketch folder; the top-level `midi_expression.ino` is a copy).

**Test sketches** (standalone, upload individually for hardware bringup):
- `test_display.ino` — OLED on Wire2
- `test_rotary.ino` — KY-040 encoder (raw ISR version)
- `test_rotary_display.ino` — encoder + OLED combined
- `test_expression.ino` — single expression pedal minimal sketch

**Python MIDI monitor** (`midi_monitor.py`): runs on a Raspberry Pi Zero 2W with an Adafruit Mini PiTFT 135×240 (ST7789). Install deps: `sudo apt install python3-pip fonts-dejavu && pip3 install adafruit-circuitpython-rgb-display mido python-rtmidi pillow`. Run with `python3 midi_monitor.py`.

## Architecture

### Main Firmware (`midi_expression.ino`)

**Per-jack state split across two structs:**
- `JackMidiConfig` — persistent settings (CC, channel, invert, wiperOnRing, calMin, calMax). Saved to EEPROM on every change. Loaded at boot; falls back to hardcoded defaults if the EEPROM magic sentinel (`0xABCE`) doesn't match.
- `JackState` — runtime state (plugged, type, settling, lastSent, calibrating, calMinSeen/calMaxSeen). Not persisted.

**Dynamic Control Surface objects via placement new:**
`CCPotentiometer` and `CCButton` objects are created into pre-allocated aligned static buffers (`j1ExprBuf`, `j1BtnBuf`, etc.) using placement new when a pedal is detected, and explicitly destroyed (calling the destructor then setting the pointer to `nullptr`) on unplug or when settings change. This lets the Control Surface library handle smoothing/filtering for expression pedals and debouncing for switches. When MIDI settings change mid-session, `rebuildExprJack()` destroys and recreates the active object.

**Plug detection and classification flow (`handleExpressionJack`):**
1. S_s pin reads HIGH when a plug is inserted (internal pull-up wins when normalling contact opens).
2. A 200 ms debounce settle period fires after every plug/unplug edge.
3. After settling, `classifyJack(sensePin, wiperPin)` averages 8 ADC samples on the sense pin (non-wiper contact). If sense > `RING_THRESHOLD` (200 ADC counts) → TRS expression pedal. Otherwise, reads the wiper pin to determine NO vs NC switch.
4. The `wiperOnRing` config field determines which physical pin is wiper vs sense.

**Rotary encoder editing (`EditField` enum, `handleEncoder`, `applyEncoderChange`):**
Push button cycles through 20 editable fields (5 per jack: CC, Ch, Inv, Wip, Cal). PJRC Encoder library gives 4 counts per physical detent; `encLastPos` accumulates until a full detent is crossed. Edit mode auto-times out after 10 seconds of inactivity. CAL mode tracks the min/max ADC values seen while the pedal moves; exiting CAL saves them to config and EEPROM.

**Display (`updateDisplay`, `drawJackRow`):**
Refreshes at ~10 fps. Each jack occupies two text rows (14px). Highlighted fields render with inverted colors (black-on-white). Cal mode replaces the status line with live min–max range being captured.

### Hardware Pin Map (Teensy 4.1)

| Function | Pin |
|----------|-----|
| J1 Tip (wiper/sense) | A0 (14) |
| J1 Ring (sense/wiper) | A1 (15) |
| J1 Plug-detect (S_s) | 0 |
| J2 Tip | A2 (16), Ring A3 (17), Detect 1 |
| J3 Tip | A4 (18), Ring A5 (19), Detect 2 |
| J4 Tip | A6 (20), Ring A7 (21), Detect 3 |
| Encoder CLK/DT/SW | 4, 5, 6 |
| OLED SDA2/SCL2 | 25, 24 (Wire2) |

ADC resolution is set to 12-bit (0–4095) at startup. All GPIO is 3.3 V — the Teensy 4.1 is **not** 5 V tolerant.

### EEPROM Layout

`EEPROMConfig` struct written at address 0. Magic sentinel `0xABCE` must match; bump the constant in both `EEPROM_MAGIC` and a comment when the struct layout changes to force re-initialization on existing devices.
