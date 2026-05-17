#pragma once

#include "common.h"

namespace PyMKF {

// Main generic converter processor
json process_converter(const std::string& topologyName, json converterJson, bool useNgspice = true);

// Combined endpoint: converter -> magnetic designs
json design_magnetics_from_converter(
    const std::string& topologyName, 
    json converterJson, 
    int maxResults, 
    json coreModeJson, 
    bool useNgspice = true, 
    json weightsJson = nullptr);

// Per-topology thin wrappers. useNgspice defaults to true; pass false in sandboxed
// environments where libngspice cannot dlopen.
json process_flyback(json flybackJson, bool useNgspice = true);
json process_buck(json buckJson, bool useNgspice = true);
json process_boost(json boostJson, bool useNgspice = true);
json process_single_switch_forward(json forwardJson, bool useNgspice = true);
json process_two_switch_forward(json forwardJson, bool useNgspice = true);
json process_active_clamp_forward(json forwardJson, bool useNgspice = true);
json process_push_pull(json pushPullJson, bool useNgspice = true);
json process_isolated_buck(json isolatedBuckJson, bool useNgspice = true);
json process_isolated_buck_boost(json isolatedBuckBoostJson, bool useNgspice = true);
json process_current_transformer(json ctJson, double turnsRatio,
                                  double secondaryResistance = 0.0, bool useNgspice = true);

void register_converter_bindings(py::module& m);

} // namespace PyMKF
