#!/usr/bin/env python3
"""
midi_monitor_terminal.py — Terminal MIDI monitor for Ubuntu.

Displays incoming MIDI messages to stdout with timestamps.

Requirements:
  pip install mido python-rtmidi
  (or: sudo apt install python3-mido python3-rtmidi)

Usage:
  python3 midi_monitor_terminal.py [port-name]
  python3 midi_monitor_terminal.py --list

Press Ctrl-C to exit.
"""

import sys
import time
import mido


def find_port(hint=None):
    """Return a port name matching hint, or first available, or None."""
    names = mido.get_input_names()
    if not names:
        return None
    if hint:
        for name in names:
            if hint.lower() in name.lower():
                return name
        return None
    for name in names:
        low = name.lower()
        if "teensy" in low or "midi expression" in low:
            return name
    return names[0]


def format_msg(msg):
    t = time.strftime("%H:%M:%S")
    ch = f"ch{msg.channel + 1:2d}" if hasattr(msg, "channel") else "    "

    if msg.type == "control_change":
        bar_len = int(msg.value / 127 * 20)
        bar = "#" * bar_len + "-" * (20 - bar_len)
        return f"{t}  {ch}  CC {msg.control:3d}  [{bar}] {msg.value:3d}"
    elif msg.type == "note_on":
        vel_bar = "#" * int(msg.velocity / 127 * 10)
        return f"{t}  {ch}  NOTE ON   note={msg.note:3d}  vel={msg.velocity:3d}  {vel_bar}"
    elif msg.type == "note_off":
        return f"{t}  {ch}  NOTE OFF  note={msg.note:3d}"
    elif msg.type == "pitchwheel":
        return f"{t}  {ch}  PITCH     value={msg.pitch:6d}"
    elif msg.type == "program_change":
        return f"{t}  {ch}  PROG CHG  program={msg.program}"
    elif msg.type == "clock":
        return None  # suppress clock spam
    else:
        return f"{t}  {ch}  {msg}"


def main():
    if "--list" in sys.argv:
        names = mido.get_input_names()
        if names:
            print("Available MIDI input ports:")
            for n in names:
                print(f"  {n}")
        else:
            print("No MIDI input ports found.")
        return

    hint = sys.argv[1] if len(sys.argv) > 1 else None

    print("MIDI Terminal Monitor")
    print("Press Ctrl-C to exit\n")

    while True:
        port_name = find_port(hint)
        if port_name is None:
            print("Waiting for MIDI device...", end="\r", flush=True)
            time.sleep(1)
            continue

        print(f"Connected: {port_name}\n")
        try:
            with mido.open_input(port_name) as port:
                for msg in port:
                    line = format_msg(msg)
                    if line:
                        print(line)
        except (OSError, IOError) as e:
            print(f"\nDisconnected ({e}), reconnecting...")
            time.sleep(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nDone.")
