# Midi Expression

USB MIDI pedal controller powered by an Adafruit KB2040.

## Overview

This project drives a set of 3 MIDI pedals over USB using an Adafruit KB2040 microcontroller:

1. **Soft pedal (on/off)** -- On Stage OS KSP sustain pedal
2. **Sostenuto pedal (on/off)** -- Casio SP20
3. **Sustain pedal (analog/expression)** -- Yamaha FC3A

The KB2040 enumerates as a class-compliant USB MIDI device, so no additional drivers are needed on the host. All pedal messages are hard-coded to **MIDI channel 5**.

## Hardware

Each pedal connects via a **6-pin 1/4" TRS socket** (3 sockets total). The enclosure will be labeled to indicate which pedal plugs into which port.

The board includes:

- **Hot-plug protection** -- hardware safeguards for plugging and unplugging pedals while powered.
- **TS plug protection** -- hardware protection against plugging a TS (mono) plug into the analog expression port.
- **Plug-detect** -- the software detects whether a pedal is currently plugged in, using the strategies described at:
  <https://www.roxxxtar.com/blog/articles/2023/11/01/arduino-trs-jack-for-expression-pedal-and-dual-footswitch>

## Software

The firmware is built with the [Control Surface](https://github.com/tttapa/Control-Surface) library.
