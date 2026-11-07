// Worst-case-cost annotations for virtuous-cycles' vc-cxx-cost (test/cxx-cost/run.sh).
// Under the analysis (-DVC_COST_ANALYSIS, clang 20) these are real attributes from
// <vc/cost.h>; in every firmware and host-test build they expand to nothing, so
// they add no code and no include-path requirement.
#ifndef CR_COST_H
#define CR_COST_H

#ifdef VC_COST_ANALYSIS
#include <vc/cost.h>
#else
#define VC_BOUND(...)
#define VC_COST(...)
#endif

#endif
