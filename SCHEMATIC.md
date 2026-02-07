# Wiring Schematic

Wiring guide for the Midi Expression pedal controller: three 6-pin switched
1/4" TRS sockets connected to an Adafruit KB2040.

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

## KB2040 Pin Assignments

```
Socket  Pin   KB2040   GPIO    Function                      Pin Config
──────  ────  ───────  ──────  ────────────────────────────   ──────────────
J1      T     A0       GPIO26  Expression / pedal wiper       Analog input
J1      R     A1       GPIO27  Expression VCC / pedal sense   Analog input
J1      S     GND      —       Sleeve ground                  —
J1      S_s   D2       GPIO2   Plug-detect                    INPUT_PULLUP

J2      T     A2       GPIO28  Expression / pedal wiper       Analog input
J2      R     A3       GPIO29  Expression VCC / pedal sense   Analog input
J2      S     GND      —       Sleeve ground                  —
J2      S_s   D3       GPIO3   Plug-detect                    INPUT_PULLUP

J3      T     D4       GPIO4   On/off pedal input             INPUT_PULLUP
J3      S     GND      —       Sleeve ground                  —
J3      S_s   D5       GPIO5   Plug-detect                    INPUT_PULLUP
```

All four ADC channels (A0–A3) are consumed by J1 and J2, giving both
sockets full expression-pedal capability while also supporting simple
on/off pedals.

---

## Bill of Materials

```
Ref   Value           Qty  Description
────  ──────────────  ───  ──────────────────────────────────────────────────
U1    Adafruit KB2040  1   Microcontroller (RP2040, USB-C)
J1    6-pin sw. TRS    1   1/4" socket — Sustain pedal (Yamaha FC3A)
J2    6-pin sw. TRS    1   1/4" socket — Soft pedal (On Stage KSP)
J3    6-pin sw. TRS    1   1/4" socket — Sostenuto pedal (Casio SP20)
R1    1 kΩ             1   J1 Tip series protection / LPF
R2    100 Ω            1   J1 Ring series — VCC current limit + TS protection
R3    1 kΩ             1   J2 Tip series protection / LPF
R4    100 Ω            1   J2 Ring series — VCC current limit + TS protection
R5    1 kΩ             1   J3 Tip series protection
C1    100 nF ceramic   1   J1 Tip analog smoothing  (LPF with R1, fc ≈ 1.6 kHz)
C2    100 nF ceramic   1   J1 Ring analog smoothing (LPF with R2, fc ≈ 11 kHz)
C3    100 nF ceramic   1   J2 Tip analog smoothing  (LPF with R3, fc ≈ 1.6 kHz)
C4    100 nF ceramic   1   J2 Ring analog smoothing (LPF with R4, fc ≈ 11 kHz)
C5    100 nF ceramic   1   J3 Tip debounce
C6    100 nF ceramic   1   3.3 V supply rail decoupling (near jacks)
```

---

## Circuit — J1  (Expression-Capable Socket)

Default assignment: **Sustain pedal — Yamaha FC3A**

The Yamaha FC3A is a half-damper expression pedal with an internal
potentiometer wired Tip = wiper, Ring = VCC end, Sleeve = GND end.

```
                    6-pin Switched
                    TRS Jack (J1)                          KB2040
                   ┌────────────┐                       ┌──────────┐
                   │            │  R1 [1 kΩ]            │          │
            T   ●──┤            ├────┤├────┬─────────── ┤ A0       │
                   │            │          │             │ (GPIO26) │
                   │            │         C1 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
                   │            │  R2 [100 Ω]            │          │
            R   ●──┤            ├────┤├────┬─────────── ┤ A1       │
                   │            │          │             │ (GPIO27) │
                   │            │         C2 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
          T_s   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          R_s   ●──┤            ├──── NC                 │          │
                   │            │                        │          │
            S   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          S_s   ●──┤            ├─────────────────────── ┤ D2       │
                   │            │                        │ (GPIO2)  │
                   └────────────┘                        └──────────┘
```

### J1 Wiring Notes

| Jack pin | Connection | Purpose |
|----------|------------|---------|
| T        | → R1 (1 kΩ) → A0, with C1 (100 nF) to GND | Reads expression wiper or switch state. R1 + C1 form a 1.6 kHz low-pass filter for noise rejection. R1 also limits current during hot-plug transients. |
| R        | → R2 (100 Ω) → A1, with C2 (100 nF) to GND | Reads Ring voltage. Software drives this via the ADC to detect pedal type. R2 limits short-circuit current to 33 mA if a TS plug grounds Ring. |
| T_s      | → GND | Grounds the Tip analog input when no plug is inserted, preventing a floating ADC reading. Connection breaks when plug is inserted. |
| R_s      | → NC (not connected) | Not used. |
| S        | → GND | Common ground / shield. |
| S_s      | → D2 (INPUT_PULLUP) | **Plug detect.** When no plug: S_s is shorted to S (GND) → reads **LOW**. When plug inserted: S_s disconnects → internal pull-up → reads **HIGH**. |

---

## Circuit — J2  (Expression-Capable Socket)

Default assignment: **Soft pedal — On Stage KSP**

Identical topology to J1 but on a separate pair of ADC channels.
The On Stage KSP is a normally-open sustain switch (TS plug), but
because this socket is expression-capable the software can also
accept an expression pedal here.

```
                    6-pin Switched
                    TRS Jack (J2)                          KB2040
                   ┌────────────┐                       ┌──────────┐
                   │            │  R3 [1 kΩ]            │          │
            T   ●──┤            ├────┤├────┬─────────── ┤ A2       │
                   │            │          │             │ (GPIO28) │
                   │            │         C3 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
                   │            │  R4 [100 Ω]            │          │
            R   ●──┤            ├────┤├────┬─────────── ┤ A3       │
                   │            │          │             │ (GPIO29) │
                   │            │         C4 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
          T_s   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          R_s   ●──┤            ├──── NC                 │          │
                   │            │                        │          │
            S   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          S_s   ●──┤            ├─────────────────────── ┤ D3       │
                   │            │                        │ (GPIO3)  │
                   └────────────┘                        └──────────┘
```

### J2 Wiring Notes

Same topology as J1 — see the J1 wiring table above. Substitute:
R1→R3, C1→C3, A0→A2, R2→R4, C2→C4, A1→A3, D2→D3.

---

## Circuit — J3  (Digital-Only Socket)

Default assignment: **Sostenuto pedal — Casio SP20**

The Casio SP20 is a sustain-type switch pedal (TS plug). Only the
Tip signal is read; Ring is unused.

```
                    6-pin Switched
                    TRS Jack (J3)                          KB2040
                   ┌────────────┐                       ┌──────────┐
                   │            │  R5 [1 kΩ]            │          │
            T   ●──┤            ├────┤├────┬─────────── ┤ D4       │
                   │            │          │             │ (GPIO4)  │
                   │            │         C5 [100 nF]    │          │
                   │            │          │             │          │
                   │            │         GND            │          │
                   │            │                        │          │
          T_s   ●──┤            ├──── GND                │          │
                   │            │                        │          │
            R   ●──┤            ├──── NC                 │          │
                   │            │                        │          │
          R_s   ●──┤            ├──── NC                 │          │
                   │            │                        │          │
            S   ●──┤            ├──── GND                │          │
                   │            │                        │          │
          S_s   ●──┤            ├─────────────────────── ┤ D5       │
                   │            │                        │ (GPIO5)  │
                   └────────────┘                        └──────────┘
```

### J3 Wiring Notes

| Jack pin | Connection | Purpose |
|----------|------------|---------|
| T        | → R5 (1 kΩ) → D4 (INPUT_PULLUP), with C5 (100 nF) to GND | Reads pedal switch state. Pull-up reads HIGH when open, LOW when closed. R5 limits hot-plug current; R5 + C5 debounce the switch. |
| T_s      | → GND | Grounds the input when no plug is inserted, so D4 reads a stable LOW instead of floating. |
| R        | → NC | Not used. |
| R_s      | → NC | Not used. |
| S        | → GND | Common ground. |
| S_s      | → D5 (INPUT_PULLUP) | **Plug detect.** Same logic as J1/J2: LOW = empty, HIGH = plug present. |

---

## System Overview Diagram

```
                              KB2040
                          ┌──────────────┐
                    USB ──┤ USB-C        │
                          │              │
  J1.T  ── R1 [1kΩ] ──┬──┤ A0  (GPIO26) │  Sustain pedal (Yamaha FC3A)
                    C1 ─┘  │              │
  J1.R  ── R2 [100Ω]──┬──┤ A1  (GPIO27) │
                    C2 ─┘  │              │
  J1.S ──────────── GND   │              │  Sleeve ground
  J1.S_s ─────────────────┤ D2  (GPIO2)  │  J1 plug detect
                           │              │
  J2.T  ── R3 [1kΩ] ──┬──┤ A2  (GPIO28) │  Soft pedal (On Stage KSP)
                    C3 ─┘  │              │
  J2.R  ── R4 [100Ω]──┬──┤ A3  (GPIO29) │
                    C4 ─┘  │              │
  J2.S ──────────── GND   │              │  Sleeve ground
  J2.S_s ─────────────────┤ D3  (GPIO3)  │  J2 plug detect
                           │              │
  J3.T  ── R5 [1kΩ] ──┬──┤ D4  (GPIO4)  │  Sostenuto pedal (Casio SP20)
                    C5 ─┘  │              │
  J3.S ──────────── GND   │              │  Sleeve ground
  J3.S_s ─────────────────┤ D5  (GPIO5)  │  J3 plug detect
                           │              │
              C6 ── 3V3 ──┤ 3V           │
              │            │              │
             GND ─────────┤ GND          │
                           └──────────────┘

  All T_s pins ──── GND    (ground analog/digital inputs when unplugged)
  J1.R_s, J2.R_s, J3.R, J3.R_s ──── NC
```

---

## Plug-Detect Logic

All three sockets use the **Sleeve switched (S_s)** pin for plug detection.
The detect pins are configured with internal pull-ups.

| Condition           | S_s state                            | Pin reads |
|---------------------|--------------------------------------|-----------|
| **No plug**         | S_s shorted to S (GND) via normalling contact | **LOW**  |
| **Plug inserted**   | S_s disconnected; internal pull-up wins | **HIGH** |

This is consistent across all three sockets: **HIGH = plugged in**.

---

## Protection Design

### Hot-Plug Protection

When inserting or removing a 1/4" plug, the tip can momentarily brush
across the ring and sleeve contacts, causing transient shorts.

- **R1, R3, R5 (1 kΩ)** — series resistors on every Tip input limit
  transient current to ≤ 3.3 mA, well within RP2040 GPIO limits.
- **R2, R4 (100 Ω)** — series resistors on Ring inputs limit current
  to ≤ 33 mA during momentary shorts.
- **C1–C5 (100 nF)** — bypass capacitors absorb voltage spikes.

### TS Plug Protection (J1 & J2)

If a TS (mono) plug is inserted into an expression-capable socket, the
plug's sleeve spans both the Ring and Sleeve contacts, shorting Ring
to ground.

- **R2 / R4 (100 Ω)** limits the short-circuit current from the 3.3 V
  supply (when software is driving Ring HIGH) to 33 mA — safe for the
  KB2040 regulator and GPIO.
- The ADC on the Ring pin will read near 0 V, which the software uses
  to identify the plug as TS (on/off pedal) rather than TRS (expression).

### Floating-Input Prevention

- **T_s → GND** on all three sockets grounds the Tip input when no
  plug is present, giving a clean 0 V / LOW instead of floating noise.

---

## Pedal-Type Detection (Software Strategy)

Because J1 and J2 have analog reads on both Tip and Ring, the software
can distinguish pedal types at runtime:

```
1. Check S_s → if LOW, no plug present — skip this socket.

2. Read Ring (A1 / A3):
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

- The KB2040 RP2040 ADC is 12-bit (0–4095) with a 3.3 V reference.
- All GPIO is 3.3 V — never connect 5 V signals to any pin.
- C6 (100 nF decoupling) should be placed physically close to the
  jack cluster, between the 3.3 V rail and ground.
- The enclosure should be labeled to identify each socket:
  **J1** = Sustain (expression), **J2** = Soft, **J3** = Sostenuto.
