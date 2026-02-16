# Wiring Schematic

Wiring guide for the Midi Expression pedal controller: four 6-pin switched
1/4" TRS sockets connected to a Teensy 4.1 (without Ethernet).

---

## 6-Pin Switched TRS Jack — Pin Naming Convention

Each socket has six pins. When **no plug** is inserted the switched contacts
are shorted to their corresponding signal contacts. Inserting a plug **breaks**
those connections.

```
Pin   Name              Description
────  ────────────────  ──────────────────────────────────────────
T     Tip               Signal contact — touches the tip of the plug
T_s   Tip (switched)    Normalling contact — shorted to T when empty
R     Ring              Signal contact — touches the ring of the plug
R_s   Ring (switched)   Normalling contact — shorted to R when empty
S     Sleeve            Signal contact — touches the sleeve (ground)
S_s   Sleeve (switched) Normalling contact — shorted to S when empty
```

---

## Teensy 4.1 Pin Assignments

```
Socket  Pin   Teensy   Arduino  Function                      Pin Config
──────  ────  ───────  ───────  ────────────────────────────   ──────────────
J1      T     A0       14       Expression / pedal wiper       Analog input
J1      R     A1       15       Expression VCC / pedal sense   Analog input
J1      S     GND      —        Sleeve ground                  —
J1      S_s   0        0        Plug-detect                    INPUT_PULLUP

J2      T     A2       16       Expression / pedal wiper       Analog input
J2      R     A3       17       Expression VCC / pedal sense   Analog input
J2      S     GND      —        Sleeve ground                  —
J2      S_s   1        1        Plug-detect                    INPUT_PULLUP

J3      T     A4       18       Expression / pedal wiper       Analog input
J3      R     A5       19       Expression VCC / pedal sense   Analog input
J3      S     GND      —        Sleeve ground                  —
J3      S_s   2        2        Plug-detect                    INPUT_PULLUP

J4      T     A6       20       Expression / pedal wiper       Analog input
J4      R     A7       21       Expression VCC / pedal sense   Analog input
J4      S     GND      —        Sleeve ground                  —
J4      S_s   3        3        Plug-detect                    INPUT_PULLUP

OLED    SDA   SDA2     25       SSD1306 I2C data               I2C (Wire2)
OLED    SCL   SCL2     24       SSD1306 I2C clock              I2C (Wire2)
OLED    VCC   3.3V     —        Display power                  —
OLED    GND   GND      —        Display ground                 —

ENC     CLK   4        4        Encoder A phase                Digital input
ENC     DT    5        5        Encoder B phase                Digital input
ENC     SW    6        6        Encoder push-button            INPUT_PULLUP
ENC     +     3.3V     —        Encoder power (3.3 V only!)    —
ENC     GND   GND      —        Encoder ground                 —
```

Eight of the Teensy 4.1's analog channels (A0–A7) are used by the four jacks,
giving all sockets full expression-pedal capability while also supporting
simple on/off switch pedals. The OLED display uses Wire2 (SCL2=24, SDA2=25),
freeing A4/A5 (pins 18/19) for J3.

---

## Bill of Materials

```
Ref    Value           Qty  Description
─────  ──────────────  ───  ──────────────────────────────────────────────────
U1     Teensy 4.1       1   Microcontroller (iMXRT1062, USB Micro-B)
J1     6-pin sw. TRS    1   1/4" socket — Sustain pedal (expression-capable)
J2     6-pin sw. TRS    1   1/4" socket — Soft pedal (expression-capable)
J3     6-pin sw. TRS    1   1/4" socket — Sostenuto pedal (expression-capable)
J4     6-pin sw. TRS    1   1/4" socket — Pedal 4 (expression-capable)
R1     1 kΩ             1   J1 Tip series protection / LPF
R2     1 kΩ             1   J1 Ring series protection / LPF
R3     1 kΩ             1   J2 Tip series protection / LPF
R4     1 kΩ             1   J2 Ring series protection / LPF
R5     1 kΩ             1   J3 Tip series protection / LPF
R6     1 kΩ             1   J3 Ring series protection / LPF
R7     1 kΩ             1   J4 Tip series protection / LPF
R8     1 kΩ             1   J4 Ring series protection / LPF
R9     1.2 kΩ           1   J1 S_s plug-detect series protection
R10    1.2 kΩ           1   J2 S_s plug-detect series protection
R11    1.2 kΩ           1   J3 S_s plug-detect series protection
R12    1.2 kΩ           1   J4 S_s plug-detect series protection
OLED   SSD1306 128x64   1   0.96" I2C OLED display (addr 0x3C)
ENC    KY-040           1   360° rotary encoder module with push-button
C1     100 nF ceramic   1   J1 Tip analog smoothing  (LPF with R1, fc ≈ 1.6 kHz)
C2     100 nF ceramic   1   J1 Ring analog smoothing (LPF with R2, fc ≈ 1.6 kHz)
C3     100 nF ceramic   1   J2 Tip analog smoothing  (LPF with R3, fc ≈ 1.6 kHz)
C4     100 nF ceramic   1   J2 Ring analog smoothing (LPF with R4, fc ≈ 1.6 kHz)
C5     100 nF ceramic   1   J3 Tip analog smoothing  (LPF with R5, fc ≈ 1.6 kHz)
C6     100 nF ceramic   1   J3 Ring analog smoothing (LPF with R6, fc ≈ 1.6 kHz)
C7     100 nF ceramic   1   J4 Tip analog smoothing  (LPF with R7, fc ≈ 1.6 kHz)
C8     100 nF ceramic   1   J4 Ring analog smoothing (LPF with R8, fc ≈ 1.6 kHz)
C9     100 nF ceramic   1   3.3 V supply rail decoupling (near jacks)
```

---

## Circuit — J1  (Expression-Capable Socket)

Default assignment: **Sustain pedal — Yamaha FC3A**

The Yamaha FC3A is a half-damper expression pedal with an internal
potentiometer wired Tip = wiper, Ring = VCC end, Sleeve = GND end.

```
                    6-pin Switched
                    TRS Jack (J1)                        Teensy 4.1
                   ┌────────────┐                       ┌──────────┐
                   │            │  R1 [1 kΩ]            │          │
            T   ●──┤            ├────┤├────┬─────────── ┤ A0       │
                   │            │          │             │ (pin 14) │
                   │            │         C1 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
                   │            │  R2 [1 kΩ]             │          │
            R   ●──┤            ├────┤├────┬─────────── ┤ A1       │
                   │            │          │             │ (pin 15) │
                   │            │         C2 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
          T_s   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          R_s   ●──┤            ├──── NC                 │          │
                   │            │                        │          │
            S   ●──┤            ├──── GND                │          │
                   │            │         R9 [1.2 kΩ]    │          │
          S_s   ●──┤            ├────┤├───────────────── ┤ pin 0    │
                   │            │                        │          │
                   └────────────┘                        └──────────┘
```

### J1 Wiring Notes

| Jack pin | Connection | Purpose |
|----------|------------|---------|
| T        | → R1 (1 kΩ) → A0 (pin 14), with C1 (100 nF) to GND | Reads expression wiper or switch state. R1 + C1 form a 1.6 kHz low-pass filter for noise rejection. R1 also limits current during hot-plug transients. |
| R        | → R2 (1 kΩ) → A1 (pin 15), with C2 (100 nF) to GND | Reads Ring voltage. Software uses ADC to detect pedal type. R2 + C2 form a 1.6 kHz LPF. R2 limits short-circuit current to 3.3 mA if a TS plug grounds Ring. |
| T_s      | → GND | Grounds the Tip analog input when no plug is inserted, preventing a floating ADC reading. Connection breaks when plug is inserted. |
| R_s      | → NC (not connected) | Not used. |
| S        | → GND | Common ground / shield. |
| S_s      | → R9 (1.2 kΩ) → pin 0 (INPUT_PULLUP) | **Plug detect.** R9 provides series protection. When no plug: S_s is shorted to S (GND) → reads **LOW**. When plug inserted: S_s disconnects → internal pull-up → reads **HIGH**. |

---

## Circuit — J2  (Expression-Capable Socket)

Default assignment: **Soft pedal — On Stage KSP**

Identical topology to J1 but on a separate pair of ADC channels.
The On Stage KSP is a normally-open sustain switch (TS plug), but
because this socket is expression-capable the software can also
accept an expression pedal here.

```
                    6-pin Switched
                    TRS Jack (J2)                        Teensy 4.1
                   ┌────────────┐                       ┌──────────┐
                   │            │  R3 [1 kΩ]            │          │
            T   ●──┤            ├────┤├────┬─────────── ┤ A2       │
                   │            │          │             │ (pin 16) │
                   │            │         C3 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
                   │            │  R4 [1 kΩ]             │          │
            R   ●──┤            ├────┤├────┬─────────── ┤ A3       │
                   │            │          │             │ (pin 17) │
                   │            │         C4 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
          T_s   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          R_s   ●──┤            ├──── NC                 │          │
                   │            │                        │          │
            S   ●──┤            ├──── GND                │          │
                   │            │         R10 [1.2 kΩ]   │          │
          S_s   ●──┤            ├────┤├───────────────── ┤ pin 1    │
                   │            │                        │          │
                   └────────────┘                        └──────────┘
```

### J2 Wiring Notes

Same topology as J1 — see the J1 wiring table above. Substitute:
R1→R3, C1→C3, A0→A2 (pin 16), R2→R4, C2→C4, A1→A3 (pin 17),
R9→R10, pin 0→pin 1.

---

## Circuit — J3  (Expression-Capable Socket)

Default assignment: **Sostenuto pedal — CC 66**

Identical topology to J1 and J2 but on A4/A5. Supports both expression pedals
and on/off switches.

```
                    6-pin Switched
                    TRS Jack (J3)                        Teensy 4.1
                   ┌────────────┐                       ┌──────────┐
                   │            │  R5 [1 kΩ]            │          │
            T   ●──┤            ├────┤├────┬─────────── ┤ A4       │
                   │            │          │             │ (pin 18) │
                   │            │         C5 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
                   │            │  R6 [1 kΩ]             │          │
            R   ●──┤            ├────┤├────┬─────────── ┤ A5       │
                   │            │          │             │ (pin 19) │
                   │            │         C6 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
          T_s   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          R_s   ●──┤            ├──── NC                 │          │
                   │            │                        │          │
            S   ●──┤            ├──── GND                │          │
                   │            │         R11 [1.2 kΩ]   │          │
          S_s   ●──┤            ├────┤├───────────────── ┤ pin 2    │
                   │            │                        │          │
                   └────────────┘                        └──────────┘
```

### J3 Wiring Notes

Same topology as J1/J2 — see the J1 wiring table above. Substitute:
R1→R5, C1→C5, A0→A4 (pin 18), R2→R6, C2→C6, A1→A5 (pin 19),
R9→R11, pin 0→pin 2.

---

## Circuit — J4  (Expression-Capable Socket)

Default assignment: **Pedal 4 — CC 11 (Expression)**

Identical topology to J1/J2/J3 but on A6/A7. Supports both expression pedals
and on/off switches.

```
                    6-pin Switched
                    TRS Jack (J4)                        Teensy 4.1
                   ┌────────────┐                       ┌──────────┐
                   │            │  R7 [1 kΩ]            │          │
            T   ●──┤            ├────┤├────┬─────────── ┤ A6       │
                   │            │          │             │ (pin 20) │
                   │            │         C7 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
                   │            │  R8 [1 kΩ]             │          │
            R   ●──┤            ├────┤├────┬─────────── ┤ A7       │
                   │            │          │             │ (pin 21) │
                   │            │         C8 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
          T_s   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          R_s   ●──┤            ├──── NC                 │          │
                   │            │                        │          │
            S   ●──┤            ├──── GND                │          │
                   │            │         R12 [1.2 kΩ]   │          │
          S_s   ●──┤            ├────┤├───────────────── ┤ pin 3    │
                   │            │                        │          │
                   └────────────┘                        └──────────┘
```

### J4 Wiring Notes

Same topology as J1/J2/J3 — see the J1 wiring table above. Substitute:
R1→R7, C1→C7, A0→A6 (pin 20), R2→R8, C2→C8, A1→A7 (pin 21),
R9→R12, pin 0→pin 3.

---

## Circuit — KY-040 Rotary Encoder

Used for live editing of MIDI CC numbers, channels, invert, and
calibration settings on the OLED display.

The KY-040 module has on-board 10 kΩ pull-up resistors on CLK, DT, and SW.
No external components are needed.

**IMPORTANT:** Power the module from **3.3 V**, not 5 V. The Teensy 4.1 is
not 5 V tolerant.

```
            KY-040 Module                Teensy 4.1
           ┌─────────────┐              ┌──────────┐
    CLK ●──┤             ├───────────── ┤ pin 4    │
           │             │              │          │
     DT ●──┤             ├───────────── ┤ pin 5    │
           │             │              │          │
     SW ●──┤             ├───────────── ┤ pin 6    │
           │             │              │          │
      + ●──┤             ├──── 3.3V     │          │
           │             │              │          │
    GND ●──┤             ├──── GND      │          │
           └─────────────┘              └──────────┘
```

### Encoder Wiring Notes

| Module pin | Connection | Purpose |
|------------|------------|---------|
| CLK        | → pin 4   | Quadrature A phase. Module pull-up to VCC; active LOW when contact closes. |
| DT         | → pin 5   | Quadrature B phase. Phase relationship to CLK determines rotation direction. |
| SW         | → pin 6 (INPUT_PULLUP) | Push-button. Active LOW when pressed. |
| +          | → 3.3V    | Module supply — must be 3.3 V for Teensy compatibility. |
| GND        | → GND     | Common ground. |

If rotation direction is reversed (clockwise decreases values), swap the CLK
and DT wires.

---

## System Overview Diagram

```
                            Teensy 4.1
                          ┌──────────────┐
                    USB ──┤ USB Micro-B  │
                          │              │
  J1.T  ── R1 [1kΩ] ──┬──┤ A0  (pin 14) │  Sustain pedal (Yamaha FC3A)
                    C1 ─┘  │              │
  J1.R  ── R2 [1kΩ] ──┬──┤ A1  (pin 15) │
                    C2 ─┘  │              │
  J1.S ──────────── GND   │              │  Sleeve ground
  J1.S_s ── R9 [1.2kΩ] ──┤ pin 0        │  J1 plug detect
                           │              │
  J2.T  ── R3 [1kΩ] ──┬──┤ A2  (pin 16) │  Soft pedal (expression-capable)
                    C3 ─┘  │              │
  J2.R  ── R4 [1kΩ] ──┬──┤ A3  (pin 17) │
                    C4 ─┘  │              │
  J2.S ──────────── GND   │              │  Sleeve ground
  J2.S_s ── R10[1.2kΩ] ──┤ pin 1        │  J2 plug detect
                           │              │
  J3.T  ── R5 [1kΩ] ──┬──┤ A4  (pin 18) │  Sostenuto pedal (expression-capable)
                    C5 ─┘  │              │
  J3.R  ── R6 [1kΩ] ──┬──┤ A5  (pin 19) │
                    C6 ─┘  │              │
  J3.S ──────────── GND   │              │  Sleeve ground
  J3.S_s ── R11[1.2kΩ] ──┤ pin 2        │  J3 plug detect
                           │              │
  J4.T  ── R7 [1kΩ] ──┬──┤ A6  (pin 20) │  Pedal 4 (expression-capable)
                    C7 ─┘  │              │
  J4.R  ── R8 [1kΩ] ──┬──┤ A7  (pin 21) │
                    C8 ─┘  │              │
  J4.S ──────────── GND   │              │  Sleeve ground
  J4.S_s ── R12[1.2kΩ] ──┤ pin 3        │  J4 plug detect
                           │              │
  OLED SDA ────────────────┤ SDA2(pin 25) │  SSD1306 I2C data (Wire2)
  OLED SCL ────────────────┤ SCL2(pin 24) │  SSD1306 I2C clock (Wire2)
  OLED VCC ──── 3.3V       │              │
  OLED GND ──── GND        │              │
                           │              │
  ENC CLK ─────────────────┤ pin 4        │  KY-040 rotary encoder
  ENC DT  ─────────────────┤ pin 5        │
  ENC SW  ─────────────────┤ pin 6        │
  ENC +   ──── 3.3V        │              │
  ENC GND ──── GND         │              │
                           │              │
              C9 ── 3V3 ──┤ 3.3V         │
              │            │              │
             GND ─────────┤ GND          │
                           └──────────────┘

  All T_s pins ──── GND    (ground analog/digital inputs when unplugged)
  J1.R_s, J2.R_s, J3.R_s, J4.R_s ──── NC
```

---

## Plug-Detect Logic

All four sockets use the **Sleeve switched (S_s)** pin for plug detection,
with a 1.2 kΩ series protection resistor (R9–R12) between S_s and the
Teensy digital input. The detect pins are configured with internal pull-ups.

| Condition           | S_s state                            | Pin reads |
|---------------------|--------------------------------------|-----------|
| **No plug**         | S_s shorted to S (GND) via normalling contact | **LOW**  |
| **Plug inserted**   | S_s disconnected; internal pull-up wins | **HIGH** |

This is consistent across all four sockets: **HIGH = plugged in**.

---

## Protection Design

### Hot-Plug Protection

When inserting or removing a 1/4" plug, the tip can momentarily brush
across the ring and sleeve contacts, causing transient shorts.

- **R1–R8 (1 kΩ)** — series resistors on every analog input (Tip and Ring)
  limit transient current to ≤ 3.3 mA, well within iMXRT1062 GPIO limits.
- **R9–R12 (1.2 kΩ)** — series resistors on every plug-detect input limit
  current during hot-plug transients.
- **C1–C8 (100 nF)** — bypass capacitors absorb voltage spikes and form
  1.6 kHz low-pass filters with their respective 1 kΩ resistors.

### TS Plug Protection

If a TS (mono) plug is inserted into an expression-capable socket, the
plug's sleeve spans both the Ring and Sleeve contacts, shorting Ring
to ground.

- **R2, R4, R6, R8 (1 kΩ)** limit the short-circuit current from the 3.3 V
  supply (when software is driving Ring HIGH) to 3.3 mA — safe for the
  Teensy 4.1 regulator and GPIO.
- The ADC on the Ring pin will read near 0 V, which the software uses
  to identify the plug as TS (on/off pedal) rather than TRS (expression).

### Floating-Input Prevention

- **T_s → GND** on all four sockets grounds the Tip input when no
  plug is present, giving a clean 0 V / LOW instead of floating noise.

---

## Pedal-Type Detection (Software Strategy)

All four jacks have analog reads on both Tip and Ring, so the software
can distinguish pedal types at runtime:

```
1. Check S_s → if LOW, no plug present — skip this socket.

2. Read Ring (sense pin):
   • If Ring ≈ 0 V → TS plug detected (sleeve is grounding Ring).
     Treat Tip as a digital on/off switch.
   • If Ring tracks the output voltage → TRS plug detected.
     Treat Tip as an analog expression value.

3. For on/off pedals, auto-detect polarity on first plug insertion:
   sample the switch state at detection time and invert logic if
   the pedal reads as "pressed" while at rest (handles both
   normally-open and normally-closed pedals like the On Stage KSP
   and Casio SP20 which may differ).
```

---

## Notes

- The Teensy 4.1 has two 12-bit ADCs (use `analogReadResolution(12)` for
  0–4095 range). The analog reference is fixed at 3.3 V.
- All GPIO is 3.3 V — the Teensy 4.1 is **not** 5 V tolerant.
- C9 (100 nF decoupling) should be placed physically close to the
  jack cluster, between the 3.3 V rail and ground.
- The enclosure should be labeled to identify each socket:
  **J1** = Sustain, **J2** = Soft, **J3** = Sostenuto, **J4** = Expression.
- All four jacks are expression-capable and auto-detect pedal type (TRS expression
  vs TS switch, NO vs NC polarity).
- Per-jack invert and calibration settings are saved to EEPROM and persist
  across power cycles.
