// Midi Expression — Minimal test sketch
// Teensy 4.1 (USB Type: "MIDI" in Arduino IDE / Tools menu)
//
// Single expression pedal on J1 → CC 64 (Sustain) on Channel 5.
// No plug detection, no TS/TRS classification — just reads the pot
// and sends MIDI.

#include <Control_Surface.h>

USBMIDI_Interface midi;

CCPotentiometer sustain {A0, {MIDI_CC::Damper_Pedal, Channel_5}};

void setup() {
  analogReadResolution(12);
  Control_Surface.begin();
}

void loop() {
  Control_Surface.loop();
}
