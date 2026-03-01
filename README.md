# Midi Expression

USB MIDI pedal controller powered by a Teensy 4.1.

## Overview

This project drives a set of 4 MIDI pedals over USB using a Teensy 4.1 microcontroller. All four jacks support both expression pedals and on/off switches:

1. **Sustain** — CC 64 (default)
2. **Soft** — CC 67 (default)
3. **Sostenuto** — CC 66 (default)
4. **Expression** — CC 11 (default)

The Teensy 4.1 enumerates as a class-compliant USB MIDI device, so no additional drivers are needed on the host. Default pedal messages are on **MIDI channel 5**, but the CC number and channel for each jack can be changed live using the rotary encoder.

## Hardware

Each pedal connects via a **6-pin 1/4" TRS socket** (4 sockets total). The enclosure will be labeled to indicate which pedal plugs into which port. All jacks are expression-capable and auto-detect the pedal type (TRS expression pedal or TS on/off switch).

The board includes:

- **KY-040 rotary encoder** -- push to cycle through editable fields (CC number and channel for each jack), rotate to change the value. The OLED highlights the active field and shows an EDIT indicator. Edit mode auto-exits after 10 seconds of inactivity.
- **Hot-plug protection** -- hardware safeguards for plugging and unplugging pedals while powered.
- **TS plug protection** -- hardware protection against plugging a TS (mono) plug into the analog expression port.
- **Plug-detect** -- the software detects whether a pedal is currently plugged in, using the strategies described at:
  <https://www.roxxxtar.com/blog/articles/2023/11/01/arduino-trs-jack-for-expression-pedal-and-dual-footswitch>

## User Interface

### OLED Display

The 128×64 OLED shows live status for all four jacks. Each jack occupies two text rows: a status line and a MIDI info line.

```
┌─────────────────────────┐
│ Midi Expression    EDIT │  ← "EDIT" appears (inverted) when editing
│ J1: In TRS Expr      R  │  ← wiper indicator (R/T), invert indicator (I/.)
│   CC 64 Sustain  c 5 127│  ← CC number, CC name, channel, last sent value
│ J2: Unplugged        T  │
│   CC 67 SoftPedl c 5 ---│  ← "---" when unplugged
│ J3: In TS  NC        T  │
│   CC 66 Sostnuto c 5   0│
│ J4: In TRS Expr      T I│  ← "I" = inverted output enabled
│   CC 11 Express  c 5  64│
└─────────────────────────┘
```

**Status line (top of each jack row):**

| Display        | Meaning                                      |
|----------------|----------------------------------------------|
| `Jn: Unplugged`| No pedal in socket                           |
| `Jn: In TRS Expr` | Expression pedal detected (potentiometer) |
| `Jn: In TS  NO`| TS switch detected, normally-open            |
| `Jn: In TS  NC`| TS switch detected, normally-closed          |
| `Jn: CAL n-n`  | Calibration active — shows live min–max ADC  |

Right-side indicators on the status line:

| Indicator | Meaning                                              |
|-----------|------------------------------------------------------|
| `R`       | Wiper is on the Ring contact (e.g. Yamaha FC3A)      |
| `T`       | Wiper is on the Tip contact (most other pedals)      |
| `I`       | Expression output is inverted (127→0 instead of 0→127) |
| `.`       | Invert field is selected but invert is currently off |

**MIDI info line (bottom of each jack row):**

`CC nn [Name]  c nn nnn`

- CC number and recognized name (e.g. `CC 64 Sustain`)
- Channel: `c 5` = MIDI channel 5
- Last sent CC value (0–127), or `---` if unplugged

---

### Rotary Encoder

The KY-040 encoder has two actions: **push** (button click) and **rotate**.

#### Button — cycle through edit fields

Each press advances to the next editable field in this order:

```
(off) → J1:CC → J1:Ch → J1:Inv → J1:Wip → J1:Cal →
        J2:CC → J2:Ch → J2:Inv → J2:Wip → J2:Cal →
        J3:CC → J3:Ch → J3:Inv → J3:Wip → J3:Cal →
        J4:CC → J4:Ch → J4:Inv → J4:Wip → J4:Cal → (off)
```

21 presses cycle all the way back to off. The active field is highlighted with inverted colors (black text on white background) on the display, and `EDIT` appears in the top-right corner.

Edit mode automatically exits after **10 seconds of inactivity** (no rotation or button press).

#### Rotation — change the selected value

| Field | Action per detent                                          |
|-------|------------------------------------------------------------|
| `CC`  | Increment/decrement CC number (0–127)                      |
| `Ch`  | Increment/decrement MIDI channel (1–16)                    |
| `Inv` | Toggle expression output inversion on/off                  |
| `Wip` | Toggle wiper pin between Tip (`T`) and Ring (`R`)          |
| `Cal` | Enter or exit calibration mode (see below)                 |

All changes are saved to EEPROM immediately (except during active calibration).

---

### Pedal Type Auto-Detection

The firmware automatically detects the pedal type each time a plug is inserted:

1. A 200 ms debounce period begins after every plug or unplug event.
2. After settling, the firmware takes 8 ADC samples on the **sense pin** (the non-wiper contact).
3. Classification:
   - Sense pin reads > 200 ADC counts → **TRS expression pedal**
   - Sense pin reads low but wiper pin reads mid-range (200–3895) → **TRS expression pedal**
   - Wiper pin reads near GND (< 200) → **TS switch, normally-closed (NC)**
   - Wiper pin reads near VCC (> 3895) → **TS switch, normally-open (NO)**

**The detected type is not a configurable setting** — it is determined entirely by the hardware at plug-in time.

#### Re-detecting a pedal

If the display shows the wrong type (e.g. `TS NO` when you plugged in an expression pedal):

1. **Unplug the pedal.**
2. Adjust the **Wip** setting if needed (see next section).
3. **Replug the pedal.** Detection runs again automatically.

---

### Wiper Polarity (Wip Setting)

TRS expression pedals put the wiper (variable voltage) on different contacts depending on the manufacturer:

| Setting | Meaning                        | Example pedals              |
|---------|--------------------------------|-----------------------------|
| `T`     | Wiper on Tip, sense on Ring    | Most expression pedals      |
| `R`     | Wiper on Ring, sense on Tip    | Yamaha FC3A, some Roland    |

The Wip setting determines which pin the firmware reads for the expression value **and** which pin it uses as the sense pin during detection. **If the wiper is set to the wrong contact, the pedal will be detected as a TS switch instead of a TRS expression pedal.**

To change wiper polarity:
1. Press the encoder button until the `R` or `T` indicator for that jack is highlighted.
2. Rotate the encoder to toggle.
3. **Unplug and replug the pedal** — classification only runs at plug-in time.

Factory defaults: J1 = `R` (Ring), J2/J3/J4 = `T` (Tip).

---

### Calibration

Calibration maps the pedal's actual physical range to the full 0–127 MIDI CC range, correcting for pots that don't reach the ADC rails at heel-down or toe-down.

**To calibrate a jack:**

1. Make sure the pedal is plugged in and detected as `TRS Expr`.
2. Press the encoder button until the `Cal` field for that jack is selected (`EDIT` appears; the jack row is the active one).
3. **Rotate the encoder** — the display switches to `Jn: CAL n-n` showing live ADC min and max.
4. **Slowly move the pedal through its full range** — heel all the way down, then toe all the way down. The min and max values update in real time.
5. **Rotate the encoder again** to exit calibration. The captured min/max is saved to EEPROM immediately.

While calibrating, the normal CC output continues using the previously saved calibration range. The new range takes effect when you exit.

If calibration is accidentally entered, rotate the encoder to exit — this saves whatever min/max was seen (even if the pedal wasn't moved), so re-calibrate if needed.

To reset to uncalibrated (full ADC span), set calMin = 0 and calMax = 4095 by running calibration while holding the pedal at heel-down, then immediately exiting (captures near-0), then re-entering and toe-down and exiting — or reflash the firmware with defaults.

---

### Settings and Defaults

All settings are stored in EEPROM and loaded at boot. If the EEPROM has never been written (new device) or the firmware's config format has changed, factory defaults are used:

| Jack | CC          | Channel | Wiper | Inverted |
|------|-------------|---------|-------|----------|
| J1   | 64 Sustain  | 5       | Ring  | No       |
| J2   | 67 Soft Pedal | 5     | Tip   | No       |
| J3   | 66 Sostenuto | 5      | Tip   | No       |
| J4   | 11 Expression | 5     | Tip   | No       |

Calibration defaults to the full ADC range (0–4095), which works acceptably for most pedals without calibration.

---

## Software

The firmware is built with the [Control Surface](https://github.com/tttapa/Control-Surface) library.
