# chime_red2
CHIME RED II

WARNING: Tesla coils can be dangerous. This code is experimental. Don't even try to build this code unless you understand it and the coil you will control with it.


Build with one command (arduino-cli)
====================================

This sketch ships a `sketch.yaml` with pinned build profiles. `arduino-cli`
reads it and auto-installs the board core and every library on the fly (into an
isolated per-sketch dir -- nothing global is touched):

    arduino-cli compile --profile due                 # Arduino Due (default)
    arduino-cli upload  --profile due -p /dev/ttyACM0
    arduino-cli compile --profile samd21              # SparkFun SAMD21

No Boards Manager, Library Manager, or board-index URL needed on this path.


Open and build in the Arduino IDE
=================================

NOTE: the Arduino IDE (through 2.3.6) does NOT read `sketch.yaml` -- profile
selection is not a shipped IDE feature, so the IDE will not auto-install
anything. Just opening the sketch and hitting Compile fails with a missing
`MIDI.h` because the libraries aren't installed. You install the core +
libraries once (they then show up in the IDE for good), then Compile/Upload.

One-time setup in Arduino IDE 2.3.x:

1. Install the board core (Tools > Board > Boards Manager):
   * Arduino Due:   install "Arduino SAM Boards (32-bits ARM Cortex-M3)".
   * SparkFun SAMD21: first add this URL under
     File > Preferences > "Additional boards manager URLs":
       https://raw.githubusercontent.com/sparkfun/Arduino_Boards/master/IDE_Board_Manager/package_sparkfun_index.json
     then install "SparkFun SAMD Boards" in Boards Manager.
2. Install the libraries (Tools > Manage Libraries):
   * always:               ArduinoSTL, FixedPoints, MIDI Library
   * Arduino Due only:     DueTimer
   * SparkFun SAMD21 only: SAMD_TimerInterrupt
3. Select your board, then Compile/Upload.

(If you have `arduino-cli`, it shares the IDE's data dir, so
`arduino-cli lib install "MIDI Library" FixedPoints ArduinoSTL DueTimer` etc.
installs them into the IDE too -- or just run ./build.sh which does the lot.)

You still want the `-O2` and coil-tuning steps below for real hardware.


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
