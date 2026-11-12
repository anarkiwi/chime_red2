// Worst-case-cost annotations for virtuous-cycles' vc-cxx-cost (test/cxx-cost/run.sh).
// Under the analysis (-DVC_COST_ANALYSIS, clang 20) these are real attributes from
// <vc/cost.h>; in every firmware and host-test build they expand to nothing, so
// they add no code and no include-path requirement.
#ifndef CR_COST_H
#define CR_COST_H

// cppcheck (which tries every #ifdef configuration) defines __CPPCHECK__ and
// has no <vc/cost.h>, so it takes the empty-macro branch.
#if defined(VC_COST_ANALYSIS) && !defined(__CPPCHECK__)
#include <vc/cost.h>
#else
#define VC_BOUND(...)
#define VC_COST(...)
#define VC_SIZE(...)
#define VC_TOTAL(...)
#define VC_RUN_TOTAL(...)
#define VC_TARGETS(...)
#endif

#endif
