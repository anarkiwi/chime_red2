//#pragma GCC diagnostic error "-Wall"
//#pragma GCC diagnostic error "-Werror"

// TODO: Have to hack ~/.arduino15/packages/arduino/hardware/sam/*/platform.txt, to change -Os to -O2,
// rather than following GCC pragma.
// find .arduino15/packages/arduino/hardware/sam -name platform.txt -exec perl -pi -e 's/Os/O2/g' {} \;
// #pragma GCC optimize ("-O2")
// #pragma GCC push_options

// Define when running on CR original hardware (LCD + analog pots). Off by
// default so the CI board builds (arduino:sam, SparkFun:samd) and the host unit
// tests all compile the base CRIO -- CRIOLcd's ADC setup (REG_ADC_MR free-run,
// A9..A11) is Arduino Due-only and won't build for SAMD21 or off-target. The
// CR_HOST_TEST guard keeps it off for host tests even if it is enabled here for
// a real (Due) hardware build.
#ifndef CR_HOST_TEST
//#define CR_UI 1
#endif
