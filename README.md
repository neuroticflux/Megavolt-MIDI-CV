# Megavolt-MIDI-CV
ATmega328-based modular synth MIDI-CV interface

The Megavolt is a MIDI-CV interface featuring two high quality 12-bit CV outputs for note and pitchbend, four 8-bit CV outputs for MIDI CC's, as well as GATE output, two CLOCK outputs and note glide.

12-bit CVs:
CV 1: MIDI note (with glide amount pot)
CV 2: Pitchbend

MIDI NOTE ON triggers the GATE output HIGH. MIDI NOTE OFF triggers it low.

8-bit CVs:
CV 3: CC #01 (Modwheel)
CV 4: CC #04 (Aftertouch/foot controller)
CV 5: CC #16 (General purpose)
CV 6: Velocity

The two CLOCK jacks output 1/1' and 1/16' ticks the from incoming MIDI clock.

A note stack of 4 notes allows several keys to be held down in succession and released to return to the previous note in the stack, allowing for fast playing of arpeggios etc.

The interface is made to hook up to a PSU delivering +/-15V as well as +5V (Synthesizers.com standard).
Current draw untested.

TODO:

- Eurorack compatibility
- Detect MIDI channel on first incoming message
- Reverse voltage protection diodes on input
