# Midi Expression

USB MIDI pedal controller powered by an Adafruit KB2040.

## Overview

This project drives a set of 3 MIDI pedals over USB using an Adafruit KB2040 microcontroller:

- **2 on/off pedals** -- send MIDI control-change (or note) messages for momentary/toggle switching.
- **1 expression pedal** -- reads an analog input and sends continuous MIDI control-change messages.

The KB2040 enumerates as a class-compliant USB MIDI device, so no additional drivers are needed on the host.
