// Host-test stub: on a real PC the C++ standard library already provides the
// containers and algorithms ArduinoSTL ports to AVR, so pull them in here so
// the real synth code (std::vector/stack/deque, std::min, bzero/memset)
// compiles against stock libstdc++.
#ifndef CR_HOST_ARDUINOSTL_STUB_H
#define CR_HOST_ARDUINOSTL_STUB_H
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <vector>
#endif
