#!/usr/bin/env python3
"""Print incoming MIDI, grouped by channel - a quick way to verify the
Core2 is sending.

macOS: first pair "Core2MIDI" in  Audio MIDI Setup > (window menu)
       Show MIDI Studio > Bluetooth , then run this. It auto-picks the
       first input port whose name contains "core2" (override with -p).

    pip install mido python-rtmidi
    python3 tools/midi_monitor.py
    python3 tools/midi_monitor.py -p "Core2MIDI Bluetooth"
    python3 tools/midi_monitor.py -l          # just list ports
"""
import argparse
import collections
import sys
import time

import mido


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", help="input port name (substring ok)")
    ap.add_argument("-l", "--list", action="store_true", help="list ports and exit")
    args = ap.parse_args()

    ports = mido.get_input_names()
    if args.list:
        print("\n".join(ports) or "(no MIDI input ports)")
        return

    name = args.port
    if name and name not in ports:
        match = [p for p in ports if name.lower() in p.lower()]
        name = match[0] if match else None
    if not name:
        match = [p for p in ports if "core2" in p.lower()]
        name = match[0] if match else (ports[0] if ports else None)
    if not name:
        sys.exit("no MIDI input ports - pair the device first")

    print("listening on:", name)
    counts = collections.Counter()
    with mido.open_input(name) as port:
        try:
            for msg in port:
                if msg.type in ("note_on", "note_off", "control_change", "program_change"):
                    ch = getattr(msg, "channel", -1) + 1  # 1-based like the config
                    counts[ch] += 1
                    print(f"ch {ch:2d}  {msg.type:14s}  {msg}")
                    if sum(counts.values()) % 32 == 0:
                        print("   -- per-channel totals:", dict(sorted(counts.items())))
        except KeyboardInterrupt:
            print("\nper-channel totals:", dict(sorted(counts.items())))


if __name__ == "__main__":
    main()
