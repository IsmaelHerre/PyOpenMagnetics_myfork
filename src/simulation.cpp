#include "simulation.h"

namespace PyMKF {

json simulate(json inputsJson, json magneticJson, json modelsData) {
    try {
        OpenMagnetics::Inputs inputs(inputsJson);
        OpenMagnetics::Magnetic magnetic(magneticJson);
        
        auto reluctanceModelName = OpenMagnetics::defaults.reluctanceModelDefault;
        if (!modelsData.is_null() && modelsData.find("reluctance") != modelsData.end()) {
            OpenMagnetics::from_json(modelsData["reluctance"], reluctanceModelName);
        }
        auto coreLossesModelName = OpenMagnetics::defaults.coreLossesModelDefault;
        if (!modelsData.is_null() && modelsData.find("coreLosses") != modelsData.end()) {
            OpenMagnetics::from_json(modelsData["coreLosses"], coreLossesModelName);
        }
        auto coreTemperatureModelName = OpenMagnetics::defaults.coreTemperatureModelDefault;
        if (!modelsData.is_null() && modelsData.find("coreTemperature") != modelsData.end()) {
            OpenMagnetics::from_json(modelsData["coreTemperature"], coreTemperatureModelName);
        }
        
        OpenMagnetics::MagneticSimulator magneticSimulator;
        magneticSimulator.set_core_losses_model_name(coreLossesModelName);
        magneticSimulator.set_core_temperature_model_name(coreTemperatureModelName);
        magneticSimulator.set_reluctance_model_name(reluctanceModelName);
        auto mas = magneticSimulator.simulate(inputs, magnetic);

        json result;
        to_json(result, mas);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

std::string export_magnetic_as_subcircuit(json magneticJson,
                                          std::string simulator,
                                          double frequency,
                                          double temperature,
                                          std::string mode) {
    try {
        // CircuitSimulatorExporterModels has a string-based from_json (SIMBA/NgSpice/...)
        auto simModel = json(simulator).get<OpenMagnetics::CircuitSimulatorExporterModels>();
        // CircuitSimulatorExporterCurveFittingModes does NOT — use magic_enum for the name
        auto fitModeOpt = magic_enum::enum_cast<
            OpenMagnetics::CircuitSimulatorExporterCurveFittingModes>(mode);
        if (!fitModeOpt) {
            throw std::invalid_argument(
                "Unknown mode \"" + mode + "\". "
                "Expected one of: ANALYTICAL | LADDER | FRACPOLE | ROSANO | ROSANO_RLC | AUTO");
        }
        return OpenMagnetics::CircuitSimulatorExporter(simModel)
            .export_magnetic_as_subcircuit(OpenMagnetics::Magnetic(magneticJson),
                                           frequency, temperature,
                                           std::nullopt,   // outputFilename — don't write to file
                                           std::nullopt,   // filePathOrFile
                                           *fitModeOpt);
    }
    catch (const std::exception &exc) {
        return "Exception: " + std::string{exc.what()};
    }
}

json mas_autocomplete(json masJson, json configuration) {
    try {
        OpenMagnetics::Mas mas(masJson);
        auto completedMas = OpenMagnetics::mas_autocomplete(mas, configuration);
        json result;
        to_json(result, completedMas);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json magnetic_autocomplete(json magneticJson, json configuration) {
    try {
        OpenMagnetics::Magnetic magnetic(magneticJson);
        auto completedMagnetic = OpenMagnetics::magnetic_autocomplete(magnetic, configuration);
        json result;
        to_json(result, completedMagnetic);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json process_inputs(json inputsJson) {
    try {
        OpenMagnetics::Inputs inputs(inputsJson);
        auto operatingPoints = inputs.get_mutable_operating_points();
        for (size_t operatingPointIndex = 0; operatingPointIndex < operatingPoints.size(); ++operatingPointIndex) {
            auto excitationsPerWinding = operatingPoints[operatingPointIndex].get_excitations_per_winding();
            for (size_t excitationIndex = 0; excitationIndex < excitationsPerWinding.size(); ++excitationIndex) {
                auto excitation = operatingPoints[operatingPointIndex].get_mutable_excitations_per_winding()[excitationIndex];
                if (excitation.get_current()) {
                    auto current = excitation.get_current().value();
                    if (!current.get_processed() && current.get_waveform()) {
                        auto processed = OpenMagnetics::Inputs::calculate_basic_processed_data(current.get_waveform().value());
                        current.set_processed(processed);
                    }
                    if (!current.get_harmonics() && current.get_waveform()) {
                        auto harmonics = OpenMagnetics::Inputs::calculate_harmonics_data(current.get_waveform().value(), excitation.get_frequency());
                        current.set_harmonics(harmonics);
                    }
                    if (!current.get_waveform() && current.get_processed()) {
                        auto waveform = OpenMagnetics::Inputs::create_waveform(current.get_processed().value(), excitation.get_frequency());
                        current.set_waveform(waveform);
                    }
                    excitation.set_current(current);
                }
                if (excitation.get_voltage()) {
                    auto voltage = excitation.get_voltage().value();
                    if (!voltage.get_processed() && voltage.get_waveform()) {
                        auto processed = OpenMagnetics::Inputs::calculate_basic_processed_data(voltage.get_waveform().value());
                        voltage.set_processed(processed);
                    }
                    if (!voltage.get_harmonics() && voltage.get_waveform()) {
                        auto harmonics = OpenMagnetics::Inputs::calculate_harmonics_data(voltage.get_waveform().value(), excitation.get_frequency());
                        voltage.set_harmonics(harmonics);
                    }
                    if (!voltage.get_waveform() && voltage.get_processed()) {
                        auto waveform = OpenMagnetics::Inputs::create_waveform(voltage.get_processed().value(), excitation.get_frequency());
                        voltage.set_waveform(waveform);
                    }
                    excitation.set_voltage(voltage);
                }
                operatingPoints[operatingPointIndex].get_mutable_excitations_per_winding()[excitationIndex] = excitation;
            }
            inputs.get_mutable_operating_points()[operatingPointIndex] = operatingPoints[operatingPointIndex];
        }
        json result;
        to_json(result, inputs);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json extract_operating_point(json fileJson, size_t numberWindings, double frequency, double desiredMagnetizingInductance, json mapColumnNamesJson) {
    try {
        std::vector<std::map<std::string, std::string>> mapColumnNames = mapColumnNamesJson.get<std::vector<std::map<std::string, std::string>>>();
        auto reader = OpenMagnetics::CircuitSimulationReader(fileJson);
        auto operatingPoint = reader.extract_operating_point(numberWindings, frequency, mapColumnNames);
        operatingPoint = OpenMagnetics::Inputs::process_operating_point(operatingPoint, desiredMagnetizingInductance);
        json result;
        to_json(result, operatingPoint);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json extract_map_column_names(json fileJson, size_t numberWindings, double frequency) {
    try {
        auto reader = OpenMagnetics::CircuitSimulationReader(fileJson);
        auto columnNames = reader.extract_map_column_names(numberWindings, frequency);
        json result = json::array();
        for (auto& columnName : columnNames) {
            json aux;
            for (auto& [signal, name] : columnName) {
                aux[signal] = name;
            }
            result.push_back(aux);
        }
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json extract_column_names(json fileJson) {
    try {
        auto reader = OpenMagnetics::CircuitSimulationReader(fileJson);
        auto columnNames = reader.extract_column_names();
        json result = json::array();
        for (auto& columnName : columnNames) {
            result.push_back(columnName);
        }
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_inductance_matrix(json magneticJson, double frequency, json modelsData,
                                 json operatingPointJson) {
    try {
        OpenMagnetics::Magnetic magnetic(magneticJson);

        auto reluctanceModelName = OpenMagnetics::defaults.reluctanceModelDefault;
        if (!modelsData.is_null() && modelsData.find("reluctance") != modelsData.end()) {
            OpenMagnetics::from_json(modelsData["reluctance"], reluctanceModelName);
        }

        OpenMagnetics::Inductance inductance(reluctanceModelName);
        // operatingPointJson is optional — pass it in for temperature-dependent permeability.
        ScalarMatrixAtFrequency inductanceMatrix;
        if (operatingPointJson.is_null() || operatingPointJson.empty()) {
            inductanceMatrix = inductance.calculate_inductance_matrix(magnetic, frequency);
        } else {
            OperatingPoint operatingPoint(operatingPointJson);
            inductanceMatrix = inductance.calculate_inductance_matrix(magnetic, frequency, &operatingPoint);
        }

        json result;
        to_json(result, inductanceMatrix);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_inductance_matrix_per_frequency(json magneticJson, std::vector<double> frequencies,
                                                json modelsData, json operatingPointJson) {
    try {
        OpenMagnetics::Magnetic magnetic(magneticJson);

        auto reluctanceModelName = OpenMagnetics::defaults.reluctanceModelDefault;
        if (!modelsData.is_null() && modelsData.find("reluctance") != modelsData.end()) {
            OpenMagnetics::from_json(modelsData["reluctance"], reluctanceModelName);
        }

        OpenMagnetics::Inductance inductance(reluctanceModelName);
        std::vector<ScalarMatrixAtFrequency> matrices;
        if (operatingPointJson.is_null() || operatingPointJson.empty()) {
            matrices = inductance.calculate_inductance_matrix_per_frequency(magnetic, frequencies);
        } else {
            OperatingPoint operatingPoint(operatingPointJson);
            matrices = inductance.calculate_inductance_matrix_per_frequency(magnetic, frequencies, &operatingPoint);
        }

        json result = json::array();
        for (auto& m : matrices) {
            json mj;
            to_json(mj, m);
            result.push_back(mj);
        }
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_leakage_inductance(json magneticJson, double frequency, size_t sourceIndex) {
    try {
        OpenMagnetics::Magnetic magnetic(magneticJson);

        auto leakageInductanceOutput = OpenMagnetics::LeakageInductance().calculate_leakage_inductance_all_windings(magnetic, frequency, sourceIndex);

        json result;
        to_json(result, leakageInductanceOutput);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_dc_resistance_per_winding(json coilJson, double temperature) {
    try {
        OpenMagnetics::Coil coil(coilJson, false);
        
        auto resistances = OpenMagnetics::WindingOhmicLosses::calculate_dc_resistance_per_winding(coil, temperature);

        json result = resistances;
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_resistance_matrix(json magneticJson, double temperature, double frequency) {
    try {
        OpenMagnetics::Magnetic magnetic(magneticJson);
        
        OpenMagnetics::WindingLosses windingLosses;
        auto resistanceMatrix = windingLosses.calculate_resistance_matrix(magnetic, temperature, frequency);

        json result;
        to_json(result, resistanceMatrix);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_stray_capacitance(json coilJson, json operatingPointJson, json modelsData) {
    try {
        OpenMagnetics::Coil coil(coilJson, false);
        OperatingPoint operatingPoint(operatingPointJson);
        
        auto strayCapacitanceModelName = OpenMagnetics::StrayCapacitanceModels::ALBACH;
        if (!modelsData.is_null() && modelsData.find("strayCapacitance") != modelsData.end()) {
            OpenMagnetics::from_json(modelsData["strayCapacitance"], strayCapacitanceModelName);
        }

        OpenMagnetics::StrayCapacitance strayCapacitance(strayCapacitanceModelName);
        auto strayCapacitanceOutput = strayCapacitance.calculate_capacitance(coil);

        json result;
        to_json(result, strayCapacitanceOutput);
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_maxwell_capacitance_matrix(json coilJson, json capacitanceAmongWindingsJson) {
    try {
        OpenMagnetics::Coil coil(coilJson, false);
        auto capacitanceAmongWindings = capacitanceAmongWindingsJson.get<std::map<std::string, std::map<std::string, double>>>();

        auto maxwellMatrix = OpenMagnetics::StrayCapacitance::calculate_maxwell_capacitance_matrix(coil, capacitanceAmongWindings);

        json result = json::array();
        for (const auto& matrix : maxwellMatrix) {
            json matrixJson;
            to_json(matrixJson, matrix);
            result.push_back(matrixJson);
        }
        return result;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

void register_simulation_bindings(py::module& m) {
    m.def("simulate", &simulate,
        R"pbdoc(
        Run a complete magnetic simulation for given operating conditions.
        
        Performs electromagnetic simulation including inductance calculation,
        field distribution, and performance metrics.
        
        Args:
            magnetic_json: JSON object containing magnetic component specification.
            inputs_json: JSON object containing operating points and conditions.
            models_json: JSON object specifying which models to use for simulation.
        
        Returns:
            JSON object with simulation results including outputs.
        )pbdoc");
    
    m.def("export_magnetic_as_subcircuit", &export_magnetic_as_subcircuit,
        R"pbdoc(
        Export a magnetic component as a SPICE-compatible subcircuit.

        Generates a netlist for the selected simulator. NgSpice / LtSpice / PLECS / NL5
        return standard SPICE .subckt text (QSPICE accepts SPICE .subckt natively via
        the .LIB directive). SIMBA returns AESIM Simba JSON.

        The model includes: DC winding resistance, leakage and magnetizing inductance,
        winding AC resistance (skin/proximity, via the chosen `mode`), and a core-loss
        network. Pass the operating `frequency` and `temperature` for an accurate fit.

        Args:
            magnetic_json: Magnetic component specification.
            simulator: Target simulator. One of:
                "NgSpice" (default) — ngspice .subckt, QSPICE-compatible via .LIB
                "LtSpice"             — LTspice .subckt, QSPICE-compatible via .LIB
                "PLECS"               — Plecs format
                "NL5"                 — NL5 format
                "SIMBA"               — AESIM Simba JSON (NOT SPICE text)
            frequency: Reference frequency in Hz for the AC resistance fit (default 100 kHz).
                Use the converter's switching frequency for best accuracy at fundamental.
            temperature: Winding temperature in °C for DC resistance (default 25).
            mode: AC-resistance curve-fitting mode. One of:
                "FRACPOLE" (default) — fractional-pole network, robust for skin effect
                "LADDER"              — RL ladder; strict 100 nH ≤ L ≤ 100 mH,
                                         1 mΩ ≤ R ≤ 100 Ω bounds, silently dropped if violated
                "ROSANO" / "ROSANO_RLC" — parallel R||L stages
                "AUTO"                — read `circuitSimulatorCurveFittingMode` setting
                "ANALYTICAL"          — DC only (NgSpice raises)

        Returns:
            Subcircuit definition. On error returns a string prefixed with "Exception: ".
        )pbdoc",
        py::arg("magnetic_json"),
        py::arg("simulator")   = "NgSpice",
        py::arg("frequency")   = 100000.0,
        py::arg("temperature") = 25.0,
        py::arg("mode")        = "FRACPOLE");
    
    m.def("mas_autocomplete", &mas_autocomplete,
        R"pbdoc(
        Autocomplete a partial Mas (Magnetic Assembly Specification) structure.
        
        Fills in missing fields and calculates derived values.
        
        Args:
            mas_json: Partial Mas JSON object to complete.
        
        Returns:
            Complete Mas JSON object with all fields populated.
        )pbdoc");
    
    m.def("magnetic_autocomplete", &magnetic_autocomplete,
        R"pbdoc(
        Autocomplete a partial Magnetic component specification.
        
        Fills in missing fields and calculates derived values for core and coil.
        
        Args:
            magnetic_json: Partial Magnetic JSON object to complete.
        
        Returns:
            Complete Magnetic JSON object with all fields populated.
        )pbdoc");
    
    m.def("process_inputs", &process_inputs,
        R"pbdoc(
        Process raw input data and calculate derived quantities.
        
        Calculates harmonics, processed waveform data, and validates inputs.
        This should be called before using inputs with adviser functions.
        
        Args:
            inputs_json: Raw input JSON object with operating points.
        
        Returns:
            Processed inputs JSON with calculated harmonics and processed data.
        )pbdoc");
    
    m.def("extract_operating_point", &extract_operating_point,
        R"pbdoc(
        Extract operating point from circuit simulation data.
        
        Parses circuit simulation output file and extracts waveforms.
        
        Args:
            file_json: JSON object with simulation file data.
            number_windings: Number of windings in the magnetic.
            frequency: Operating frequency in Hz.
            desired_magnetizing_inductance: Target inductance value.
            map_column_names_json: Mapping of column names to signals.
        
        Returns:
            JSON object representing the extracted operating point.
        )pbdoc");
    
    m.def("extract_map_column_names", &extract_map_column_names,
        R"pbdoc(
        Extract column name mapping from circuit simulation file.
        
        Analyzes simulation file to determine which columns correspond
        to voltage and current signals for each winding.
        
        Args:
            file_json: JSON object with simulation file data.
            number_windings: Number of windings to map.
            frequency: Operating frequency for signal identification.
        
        Returns:
            JSON array mapping signal types to column names.
        )pbdoc");
    
    m.def("extract_column_names", &extract_column_names,
        R"pbdoc(
        Extract all column names from a circuit simulation file.
        
        Args:
            file_json: JSON object with simulation file data.
        
        Returns:
            JSON array of column name strings.
        )pbdoc");
    
    m.def("calculate_inductance_matrix", &calculate_inductance_matrix,
        R"pbdoc(
        Calculate the complete inductance matrix for a magnetic component.

        Computes self-inductances (diagonal) and mutual inductances (off-diagonal) at
        the given frequency. Pass an operating_point for temperature-dependent permeability.

        Args:
            magnetic_json: Magnetic component specification.
            frequency: Operating frequency in Hz.
            models_json: Optional dict — {"reluctance": "ZHANG" | "MUEHLETHALER" | ...}.
            operating_point_json: Optional operating point for temperature-dependent
                effects. Defaults to null (uses ambient assumption).

        Returns:
            JSON object with the inductance matrix at the specified frequency.
        )pbdoc",
        py::arg("magnetic_json"), py::arg("frequency"), py::arg("models_json"),
        py::arg("operating_point_json") = json());

    m.def("calculate_inductance_matrix_per_frequency", &calculate_inductance_matrix_per_frequency,
        R"pbdoc(
        Calculate inductance matrices at multiple frequencies in one call.

        Useful for Bode-plot style sweeps without a Python loop.

        Args:
            magnetic_json: Magnetic component specification.
            frequencies: List of frequencies in Hz.
            models_json: Optional dict (see calculate_inductance_matrix).
            operating_point_json: Optional operating point.

        Returns:
            JSON array of inductance matrices, one per frequency.
        )pbdoc",
        py::arg("magnetic_json"), py::arg("frequencies"),
        py::arg("models_json") = json(), py::arg("operating_point_json") = json());
    
    m.def("calculate_leakage_inductance", &calculate_leakage_inductance,
        R"pbdoc(
        Calculate leakage inductance between windings.
        
        Computes the leakage inductance from a source winding to all other windings.
        
        Args:
            magnetic_json: JSON object containing magnetic component specification.
            frequency: Operating frequency in Hz.
            source_index: Index of the source winding (0-based).
        
        Returns:
            JSON object with leakage inductance values to each winding.
        )pbdoc");
    
    m.def("calculate_dc_resistance_per_winding", &calculate_dc_resistance_per_winding,
        R"pbdoc(
        Calculate DC resistance for each winding.
        
        Computes the DC resistance of each winding based on wire properties
        and temperature.
        
        Args:
            coil_json: JSON object containing coil specification with turns and wire info.
            temperature: Temperature in degrees Celsius for resistance calculation.
        
        Returns:
            JSON array with DC resistance value for each winding in Ohms.
        )pbdoc");
    
    m.def("calculate_resistance_matrix", &calculate_resistance_matrix,
        R"pbdoc(
        Calculate the resistance matrix for a magnetic component.
        
        Computes the frequency-dependent resistance matrix including self and mutual
        resistances. Uses the Spreen (1990) method with inductance ratio scaling
        for proper mutual resistance calculation.
        
        Args:
            magnetic_json: JSON object containing magnetic component specification.
            temperature: Temperature in degrees Celsius for resistance calculation.
            frequency: Operating frequency in Hz.
        
        Returns:
            JSON object with resistance matrix (magnitude and frequency).
        )pbdoc");
    
    m.def("calculate_stray_capacitance", &calculate_stray_capacitance,
        R"pbdoc(
        Calculate stray capacitance for a coil.
        
        Computes turn-to-turn and winding-to-winding capacitances using the
        specified capacitance model.
        
        Args:
            coil_json: JSON object containing coil specification with turns.
            operating_point_json: JSON object with operating conditions for voltage distribution.
            models_json: JSON object specifying capacitance model (e.g., "Albach", "Koch").
        
        Returns:
            JSON object with capacitance values including capacitance among turns,
            capacitance among windings, and Maxwell capacitance matrix.
        )pbdoc");
    
    m.def("calculate_maxwell_capacitance_matrix", &calculate_maxwell_capacitance_matrix,
        R"pbdoc(
        Calculate Maxwell capacitance matrix from inter-winding capacitances.
        
        Converts inter-winding capacitance values to the standard Maxwell
        capacitance matrix format used in circuit simulation.
        
        Args:
            coil_json: JSON object containing coil specification.
            capacitance_among_windings_json: JSON object with capacitance values
                between each pair of windings.
        
        Returns:
            JSON array containing the Maxwell capacitance matrix.
        )pbdoc");
}

} // namespace PyMKF
