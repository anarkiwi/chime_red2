# MIDI and CHIME RED II

## Overview

CR2 is controllable with MIDI, by a DAW such as Ableton Live or a standalone controller (such as a keyboard, with a MIDI output connection).

* CR2 is polyphonic. There are 16 oscillators available to play notes - so up to 16 notes may be played simultaneously. However if detune parameters are used (see below) an additional oscillator may be assigned per note - so if detuning is in use, up to 8 notes may be played simultaneously.
* CR2 responds on multiple MIDI channels - on channels 1 through 8, and on channel 10. This is primarily so that the user can apply channel-wide parameter changes, such as MIDI pitchbends, on one channel while playing notes on another channel with different or default parameters.
* Channel 10 is dedicated to percussion effects (instead of notes, it plays pitched noise - the noise is generated by random pitch changes +/- 12 semitones from the note played).
* MIDI note 0 - the lowest possible note, is 1Hz, not ~8Hz per the MIDI standard. This is to achieve a percussive "tick" effect.
* CR2 is velocity sensitive (larger velocity values will play louder). CR2 does not support aftertouch.
* CR2 has 3 LFOs. One (the "configurable" LFO) is reserved for future use. The other two are for vibrato and tremolo effects. The LFOs globally affect all channels that have their vibrato or tremolo range set (in other words, if two channels have a non-zero vibrato range set, and the vibrato LFO frequency is changed, both channels will be affected).
* CR2 does not currently use clock messages (they are ignored, but may be used in the future to synchronize LFOs - see below).
* CR2 does not currently support saving channel parameters or MIDI sysex.
* CR2 does not use MIDI RPN or NRPN messages (but it is possible to set the pitch bend range - see below).

## Best practices

* If your DAW or controller can send program changes, then send a program change (the number does not matter) to reset all CC values and pitchbend to defaults.
* Sending MIDI CC 30 (the value does not matter) will do the same reset as a program change,
* If you don't reset CC values and pitchbends via one of the above methods, then when automating use of the MIDI CC messages specified below or pitchbends, ensure that you explicitly automate starting, and returning the CC value to its default (ie. in Ableton, the automation line for the CC value should be set back to default at the end of a MIDI clip). This ensures that the CC value will always be consistent whenever the DAW or CR2 are restarted.
* Setting ADSR envelope values should be done before sending a MIDI note. Sending ADSR values while a note is playing will have no effect.
* Avoid holding notes more than 2-3s. Depending on your coil, you may be controlling many kW of power! It's easy to use that power but continuous long notes (especially higher notes, over 440Hz), can stressful for the coil and its power supply.


## Supported MIDI CC messages

| CC  | Default | Description | Notes
| --- | --- | --- | ---
|123|0|All notes off
|121|0|Reset all CC values on this channel to defaults
|95|64|2nd oscillator detune in cents|If not 64, assign a second oscillator to each note, and detune it n cents from the note's fundamental frequency (e.g. 63 is -1 cents, 65 is +1 cents).
|94|64|Oscillator detune in cents|If not 64, detune note oscillator n cents from the note's fundamental frequency (e.g. 63 is -1 cents, 65 is +1 cents).
|92|0|Tremolo LFO range|Set the level this channel's notes will be affected by the tremolo LFO (0 is no tremolo).
|90|64|2nd oscillator fine detune in 20us periods|If not 64, assign a second oscillator to each note, and fine detune it by n 20 microsecond periods from the note's fundamental frequency (e.g. 63 is -1 periods 65, is +1 periods).
|89|64|Oscillator fine detune in 20us periods|If not 64, detune oscillator n 20 microsecond periods from the note's fundamental frequency (e.g. 63 is -1 periods 65, is +1 periods).
|87|0|LFO restart on note on|If 0, restart LFOs on each note on. If 1, LFOs free run.
|86|0|Vibrato LFO shape| 0 sine, 1 downward saw, 2 upward saw.
|85|0|Tremolo LFO shape| 0 sine, 1 downward saw, 2 upward saw.
|76|0|Vibrato LFO frequency| 0-127 is 0 to 10Hz
|75|0|ADSR decay time| 0-127 is 0 to 4s, as an exponential curve.
|73|0|ADSR attack time| 0-127 is 0 to 4s, as an expotential curve.
|72|0|ADSR release time| 0-127 is 0 to 4s, as an expotential curve.
|31|12|Pitchbend range in semitones| Normally set by an RPN, but DAWs such as Ableton do not natively support sending RPNs.
|30|0|Reset all CC values (same as 121 - some DAWs such as Ableton do not allow manually sending CC 121).
|24|127|ADSR sustain level|
|22|0|Tremolo LFO frequency| 0-127 is 0 to 10Hz
|7|127|Volume of all notes on this channel
|1|0|Vibrato LFO range|Set the level this channel's notes will be affected by the vibrato LFO (0 is no vibrato).






