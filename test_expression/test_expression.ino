// Midi Expression — Minimal test sketch
// Teensy 4.1 (USB Type: "MIDI" in Arduino IDE / Tools menu)
//
// Single expression pedal on J1 — Yamaha FC3A (wiper on Ring = A1)
// Sends CC 64 (Sustain) on Channel 5.
// No plug detection, no TS/TRS classification — just reads the pot
// and sends MIDI.

#include <Control_Surface.h>

USBMIDI_Interface midi;

//CCPotentiometer sustain {A1, {MIDI_CC::Damper_Pedal, Channel_5}, {0, 1023, 127, 0}};

// We define a filtered analog input that sends CC messages
// PBPotentiometer is often more "stable" for the compiler than the raw Analog template
CCPotentiometer pedal = {
  A1,
  {64, Channel_5}
};

void setup() {
  pinMode(A0, OUTPUT);
  digitalWrite(A0, HIGH);
  analogReadResolution(12);
  pinMode(A0, OUTPUT);
  digitalWrite(A0, HIGH);  // drive Tip HIGH to power the FC3A potentiometer
  Control_Surface.begin();
  pedal.invert();
  }

void loop() {
  Control_Surface.loop();

}