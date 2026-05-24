//#pragma GCC diagnostic error "-Wall"
//#pragma GCC diagnostic error "-Werror"

// TODO: Have to hack ~/.arduino15/packages/arduino/hardware/sam/*/platform.txt, to change -Os to -O2,
// rather than following GCC pragma.
// find .arduino15/packages/arduino/hardware/sam -name platform.txt -exec perl -pi -e 's/Os/O2/g' {} \;
// #pragma GCC optimize ("-O2")
// #pragma GCC push_options

// Define when running on CR original hardware.
// Host unit tests (-DCR_HOST_TEST) use the base CRIO instead, which has no LCD
// or ADC hardware, so they can build and run off-target.
#ifndef CR_HOST_TEST
#define CR_UI 1
#endif
