#pragma once

#include "common.h"

namespace PyMKF {

// Core adviser
json calculate_advised_cores(json inputsJson, json weightsJson, int maximumNumberResults, json coreModeJson);

// Magnetic adviser
//   weightsJson keys (case-sensitive): "cost", "dimensions", "efficiency", "losses",
//   plus any other MagneticFilters enum value. Pass an empty dict or null for defaults.
json calculate_advised_magnetics(json inputsJson, int maximumNumberResults,
                                 json coreModeJson, json weightsJson = json());
json calculate_advised_magnetics_fast(json inputsJson, int maximumNumberResults, json coreModeJson);
json calculate_advised_magnetics_from_catalog(json inputsJson, json catalogJson, int maximumNumberResults);
json calculate_advised_magnetics_from_cache(json inputsJson, json filterFlowJson, int maximumNumberResults);

void register_adviser_bindings(py::module& m);

} // namespace PyMKF
