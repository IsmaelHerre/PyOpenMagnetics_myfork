#pragma once

#include "common.h"

namespace PyMKF {

// Simulation
json simulate(json inputsJson, json magneticJson, json modelsData);

// Export
//   simulator:   "NgSpice" (default) | "LtSpice" | "PLECS" | "NL5" | "SIMBA"
//                NgSpice/LtSpice are QSPICE-compatible via .LIB; SIMBA returns JSON.
//   frequency:   reference frequency (Hz) for AC resistance ladder fit. Default 100 kHz.
//   temperature: winding temperature (°C) for DC resistance. Default 25.
//   mode:        curve-fitting mode for winding AC resistance:
//                "FRACPOLE" (default, robust for skin effect)
//                "LADDER"   — strict bounds, may silently drop ladder
//                "ROSANO" / "ROSANO_RLC"
//                "AUTO"     — read circuitSimulatorCurveFittingMode setting
std::string export_magnetic_as_subcircuit(json magneticJson,
                                          std::string simulator  = "NgSpice",
                                          double frequency       = 100000.0,
                                          double temperature     = 25.0,
                                          std::string mode       = "FRACPOLE");

// Autocomplete
json mas_autocomplete(json masJson, json configuration);
json magnetic_autocomplete(json magneticJson, json configuration);

// Input processing
json process_inputs(json inputsJson);
json extract_operating_point(json fileJson, size_t numberWindings, double frequency, double desiredMagnetizingInductance, json mapColumnNamesJson);
json extract_map_column_names(json fileJson, size_t numberWindings, double frequency);
json extract_column_names(json fileJson);

// Inductance calculations
json calculate_inductance_matrix(json magneticJson, double frequency, json modelsData);
json calculate_leakage_inductance(json magneticJson, double frequency, size_t sourceIndex);

// Resistance calculations
json calculate_dc_resistance_per_winding(json coilJson, double temperature);
json calculate_resistance_matrix(json magneticJson, double temperature, double frequency);

// Capacitance calculations
json calculate_stray_capacitance(json coilJson, json operatingPointJson, json modelsData);
json calculate_maxwell_capacitance_matrix(json coilJson, json capacitanceAmongWindingsJson);

void register_simulation_bindings(py::module& m);

} // namespace PyMKF
