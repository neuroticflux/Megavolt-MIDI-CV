# Megavolt-MIDI-CV
ATmega328-based modular synth MIDI-CV interface

The Megavolt is a MIDI-CV interface featuring two high quality 12-bit CV outputs for note and pitchbend, four 8-bit CV outputs for MIDI CC's, as well as two GATE outputs and note glide.

Outputs are hardcoded and can be changed by altering the sourcecode. Future versions may have some sort of on-board configuration option.

12-bit CVs:
CV 1: MIDI note (pot controls glide)
CV 2: Pitchbend

8-bit CV:
CV 3: CC #01 (Modwheel)
CV 4: CC #04 (Aftertouch/foot controller)
CV 5: CC #16 (General purpose)
CV 6: Velocity

The interface is made to hook up to a PSU delivering +/-15V as well as +5V (Synthesizers.com standard).
Current draw untested.

The Megavolt interface is in BETA so expect things to change. It requires some SMD soldering skills to put together, but uses through-hole components for IC's and all 0805 SMD components which should be easy enough even for newcomers to synth-DIY.

TODO: BOM.
