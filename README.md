# chime_red2
CHIME RED II

WARNING: Tesla coils can be dangerous. This code is experimental. Don't even try to build this code unless you understand it and the coil you will control with it.


Build and run
=============

* Select Due board, exit IDE
* Edit ~/.arduino15/packages/arduino/hardware/sam/*/platform.txt, to change -Os to -O2 (TODO: test pragma override)
* Re-enter IDE, install libraries ArduinoSTL, DueTimer, FixedPoints, MIDI Library
* Define CR_UI in chime_red2.ino if running on original CHIME RED I hardware
* Change pulseWindowUs and breakoutUs in constants.h to suit coil hardare (e.g. set pulseWindowUs to 700 for giant coils; breakoutUs sets absolute minimum gate on time)
* Upload to CHIME RED I/II


Render a MIDI file to audio (software simulation)
=================================================

You can hear what a patch will do without any hardware: the `cr-render` tool runs
the *real* synth code on your PC and renders a Standard MIDI File (`.mid`) to a
WAV (it models the coil gate output). Everything runs in Docker -- no Arduino
toolchain or local build needed, only Docker.

Simplest (the wrapper builds the image and renders in one step):

```
./test/host/render.sh song.mid song.wav
```

Renderer options pass straight through, e.g. add a 2 s release tail, boost the
gain, and resample to 44.1 kHz:

```
./test/host/render.sh song.mid song.wav --tail 2 --gain 1.5 --rate 44100
```

Or build the image once and call `cr-render` directly (handy in scripts). This
mounts the current directory as `/io`, so `song.mid` must be in it:

```
docker build -f test/host/Dockerfile -t chime-red-host-tests .
docker run --rm -v "$PWD:/io" chime-red-host-tests cr-render /io/song.mid /io/song.wav
```

Renderer options:

```
--tail SECONDS     release/silence tail after the last event (default 1.0)
--seconds SECONDS  cap total render length
--gain G           output gain multiplier (default 1.0)
--rate HZ          output sample rate (default native 52631 Hz; resamples)
--raw              emit the literal unipolar coil gate (no DC-block)
--no-normalize     do not peak-normalise the output
--seed N           RNG seed for channel-10 pitched noise (default 1)
```
