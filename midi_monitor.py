#!/usr/bin/env python3
"""
midi_monitor.py — MIDI "top" display for Raspberry Pi Zero 2W
with Adafruit Mini PiTFT 135x240 (ST7789).

Shows live MIDI CC bar graphs and a scrolling message log from the
Teensy 4.1 USB MIDI pedal controller.

Hardware:
  Adafruit Mini PiTFT 1.14" 135x240  (Product ID 4393)
    Display SPI : CE0, MOSI, SCK, DC=GPIO25
    Backlight   : GPIO22
    Button A    : GPIO23  (active LOW, 10K pull-up)
    Button B    : GPIO24  (active LOW, 10K pull-up)

Requirements:
  sudo apt install python3-pip fonts-dejavu
  pip3 install adafruit-circuitpython-rgb-display mido python-rtmidi pillow

Usage:
  python3 midi_monitor.py

Buttons:
  A (GPIO23) — clear the message log
  B (GPIO24) — reset CC values to zero
"""

import time
import threading
from collections import deque

import board
import digitalio
from adafruit_rgb_display import st7789
from PIL import Image, ImageDraw, ImageFont
import mido

# ── Display setup ───────────────────────────────────────────────────
WIDTH = 240
HEIGHT = 135

cs_pin = digitalio.DigitalInOut(board.CE0)
dc_pin = digitalio.DigitalInOut(board.D25)

BAUDRATE = 24000000

spi = board.SPI()
disp = st7789.ST7789(
    spi,
    cs=cs_pin,
    dc=dc_pin,
    rst=None,
    baudrate=BAUDRATE,
    width=135,
    height=240,
    x_offset=53,
    y_offset=40,
    rotation=270,
)

# Backlight on
backlight = digitalio.DigitalInOut(board.D22)
backlight.direction = digitalio.Direction.OUTPUT
backlight.value = True

# ── Buttons ─────────────────────────────────────────────────────────
btn_a = digitalio.DigitalInOut(board.D23)
btn_a.direction = digitalio.Direction.INPUT

btn_b = digitalio.DigitalInOut(board.D24)
btn_b.direction = digitalio.Direction.INPUT

# ── Font ────────────────────────────────────────────────────────────
FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
try:
    font = ImageFont.truetype(FONT_PATH, 10)
    font_sm = ImageFont.truetype(FONT_PATH, 9)
except OSError:
    font = ImageFont.load_default()
    font_sm = font

# ── MIDI state ──────────────────────────────────────────────────────
CHANNEL = 4  # zero-indexed → MIDI channel 5

cc_info = {
    64: {"name": "Sustain",  "value": 0, "color": (0, 255, 128)},
    67: {"name": "Soft",     "value": 0, "color": (128, 200, 255)},
    66: {"name": "Sostnuto", "value": 0, "color": (255, 200, 80)},
}

LOG_SIZE = 5
message_log = deque(maxlen=LOG_SIZE)
msg_count = 0
msg_rate = 0.0
last_rate_time = time.monotonic()
midi_connected = False

# ── Colors ──────────────────────────────────────────────────────────
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
GRAY = (50, 50, 50)
DIM = (100, 100, 100)
GREEN = (0, 200, 0)
RED = (255, 60, 60)

# ── Find Teensy MIDI port ──────────────────────────────────────────
def find_midi_port():
    """Return the first MIDI input port name that looks like a Teensy,
    or the first available port, or None."""
    names = mido.get_input_names()
    for name in names:
        lower = name.lower()
        if "teensy" in lower or "midi expression" in lower:
            return name
    # Fall back to first available port
    return names[0] if names else None

# ── MIDI reader thread ─────────────────────────────────────────────
def midi_reader():
    global midi_connected, msg_count

    while True:
        port_name = find_midi_port()
        if port_name is None:
            midi_connected = False
            time.sleep(1)
            continue

        try:
            with mido.open_input(port_name) as port:
                midi_connected = True
                print(f"Opened MIDI port: {port_name}")
                for msg in port:
                    if msg.type == "control_change" and msg.channel == CHANNEL:
                        cc = msg.control
                        val = msg.value
                        if cc in cc_info:
                            cc_info[cc]["value"] = val
                        ts = time.strftime("%H:%M:%S")
                        message_log.append(
                            f"{ts} CC{cc:3d} {val:3d} ch{msg.channel + 1}"
                        )
                        msg_count += 1
        except (OSError, IOError) as e:
            print(f"MIDI port lost ({e}), reconnecting...")
            midi_connected = False
            time.sleep(1)

# ── Drawing ─────────────────────────────────────────────────────────
BAR_X = 82       # left edge of bar graph
BAR_W = 110      # bar width in pixels
BAR_H = 10       # bar height
CC_Y_START = 20  # first CC row y position
CC_ROW_H = 16    # row spacing

def draw_frame():
    global msg_rate, msg_count, last_rate_time

    # Calculate message rate
    now = time.monotonic()
    dt = now - last_rate_time
    if dt >= 1.0:
        msg_rate = msg_count / dt
        msg_count = 0
        last_rate_time = now

    img = Image.new("RGB", (WIDTH, HEIGHT), BLACK)
    d = ImageDraw.Draw(img)

    # ── Header ──────────────────────────────────────────────────────
    d.text((2, 2), "MIDI Monitor", font=font, fill=WHITE)
    if midi_connected:
        d.text((120, 3), "CONNECTED", font=font_sm, fill=GREEN)
    else:
        d.text((120, 3), "WAITING...", font=font_sm, fill=RED)
    d.text((205, 3), f"{msg_rate:2.0f}/s", font=font_sm, fill=DIM)
    d.line([(0, 15), (WIDTH, 15)], fill=GRAY)

    # ── CC bar graphs ───────────────────────────────────────────────
    y = CC_Y_START
    for cc_num in [64, 67, 66]:
        info = cc_info[cc_num]
        val = info["value"]
        color = info["color"]

        # Label
        label = f"CC{cc_num} {info['name']}"
        d.text((2, y), label, font=font_sm, fill=DIM)

        # Bar background
        d.rectangle(
            [(BAR_X, y + 1), (BAR_X + BAR_W, y + BAR_H)],
            fill=GRAY,
        )
        # Bar fill
        fill_w = int(val / 127 * BAR_W)
        if fill_w > 0:
            d.rectangle(
                [(BAR_X, y + 1), (BAR_X + fill_w, y + BAR_H)],
                fill=color,
            )
        # Value
        d.text((BAR_X + BAR_W + 4, y), f"{val:3d}", font=font_sm, fill=WHITE)

        y += CC_ROW_H

    # ── Separator ───────────────────────────────────────────────────
    sep_y = y + 2
    d.line([(0, sep_y), (WIDTH, sep_y)], fill=GRAY)

    # ── Message log ─────────────────────────────────────────────────
    y = sep_y + 4
    d.text((2, y), "Recent:", font=font_sm, fill=DIM)
    y += 12
    for entry in reversed(message_log):
        if y + 10 > HEIGHT:
            break
        d.text((2, y), entry, font=font_sm, fill=WHITE)
        y += 11

    disp.image(img)

# ── Main loop ───────────────────────────────────────────────────────
def main():
    print("MIDI Monitor — Adafruit Mini PiTFT 135x240")
    print("Listening for Teensy USB MIDI on channel 5")
    print("Button A: clear log  |  Button B: reset values")
    print("Press Ctrl-C to exit")

    # Start MIDI reader in background
    reader = threading.Thread(target=midi_reader, daemon=True)
    reader.start()

    try:
        while True:
            # Button A — clear log
            if not btn_a.value:
                message_log.clear()
                time.sleep(0.2)

            # Button B — reset CC values
            if not btn_b.value:
                for info in cc_info.values():
                    info["value"] = 0
                time.sleep(0.2)

            draw_frame()
            time.sleep(0.05)  # ~20 fps

    except KeyboardInterrupt:
        disp.image(Image.new("RGB", (WIDTH, HEIGHT), BLACK))
        backlight.value = False
        print("\nDone.")

if __name__ == "__main__":
    main()
