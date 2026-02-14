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

## Software

The firmware is built with the [Control Surface](https://github.com/tttapa/Control-Surface) library.
