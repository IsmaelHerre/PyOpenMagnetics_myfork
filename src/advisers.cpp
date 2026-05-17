#include "advisers.h"

namespace PyMKF {

// Build a "result" entry common to all adviser functions.
// `scoringsPerFilter` is empty for fast adviser; otherwise looked up by manufacturer ref.
static json _build_adviser_result(OpenMagnetics::Mas& masMagnetic, double scoring,
        const std::map<std::string, std::map<OpenMagnetics::MagneticFilters, double>>* scoringsPerFilter = nullptr) {
    json result;
    json masJson;
    to_json(masJson, masMagnetic);
    result["mas"] = masJson;
    result["scoring"] = scoring;
    if (scoringsPerFilter) {
        auto _mi = masMagnetic.get_magnetic().get_manufacturer_info();
        std::string name = (_mi.has_value() && _mi.value().get_reference().has_value())
                            ? _mi.value().get_reference().value() : "";
        if (scoringsPerFilter->count(name)) {
            json filterScorings;
            for (auto& [filter, filterScore] : scoringsPerFilter->at(name)) {
                filterScorings[std::string(magic_enum::enum_name(filter))] = filterScore;
            }
            result["scoringPerFilter"] = filterScorings;
        }
    }
    return result;
}

// Map a Python weights dict {"cost": w, ...} to MagneticFilters enum keys, returning
// an empty map if the dict is null. Unknown keys are silently dropped (matches AGENTS.md
// "fix the JSON" model — the caller can inspect a known-good key list).
static std::map<OpenMagnetics::MagneticFilters, double> _parse_magnetic_weights(const json& weightsJson) {
    std::map<OpenMagnetics::MagneticFilters, double> weights;
    if (weightsJson.is_null() || !weightsJson.is_object()) return weights;
    for (auto& [name, w] : weightsJson.items()) {
        OpenMagnetics::MagneticFilters filter;
        try {
            OpenMagnetics::from_json(name, filter);
            weights[filter] = w.get<double>();
        } catch (...) { /* unknown filter name — skip */ }
    }
    return weights;
}

json calculate_advised_cores(json inputsJson, json weightsJson, int maximumNumberResults, json coreModeJson) {
    try {
        OpenMagnetics::Inputs inputs(inputsJson);
        OpenMagnetics::CoreAdviser::CoreAdviserModes coreMode;
        from_json(coreModeJson, coreMode);
        std::map<std::string, double> weightsKeysJson = weightsJson;
        std::map<OpenMagnetics::CoreAdviser::CoreAdviserFilters, double> weights;

        weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::COST] = 1;
        weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::EFFICIENCY] = 1;
        weights[OpenMagnetics::CoreAdviser::CoreAdviserFilters::DIMENSIONS] = 1;

        for (auto const& [filterName, weight] : weightsKeysJson) {
            OpenMagnetics::CoreAdviser::CoreAdviserFilters filter;
            OpenMagnetics::from_json(filterName, filter);
            weights[filter] = weight;
        }

        OpenMagnetics::CoreAdviser coreAdviser;
        coreAdviser.set_mode(coreMode);
        auto masMagnetics = coreAdviser.get_advised_core(inputs, weights, maximumNumberResults);

        // CoreAdviser scorings are keyed by CoreAdviserFilters (not MagneticFilters);
        // emit them via magic_enum into "scoringPerFilter" inline.
        auto scoringsPerFilter = coreAdviser.get_scorings();
        json results = json();
        results["data"] = json::array();
        for (auto& [masMagnetic, scoring] : masMagnetics) {
            auto _mi = masMagnetic.get_magnetic().get_manufacturer_info();
            std::string name = (_mi.has_value() && _mi.value().get_reference().has_value())
                                ? _mi.value().get_reference().value() : "";
            json result;
            json masJson;
            to_json(masJson, masMagnetic);
            result["mas"] = masJson;
            result["scoring"] = scoring;
            if (scoringsPerFilter.count(name)) {
                json filterScorings;
                for (auto& [filter, filterScore] : scoringsPerFilter[name]) {
                    filterScorings[std::string(magic_enum::enum_name(filter))] = filterScore;
                }
                result["scoringPerFilter"] = filterScorings;
            }
            results["data"].push_back(result);
        }

        sort(results["data"].begin(), results["data"].end(), [](json& b1, json& b2) {
            return b1["scoring"] > b2["scoring"];
        });
        return results;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_advised_magnetics(json inputsJson, int maximumNumberResults, json coreModeJson, json weightsJson) {
    try {
        OpenMagnetics::Inputs inputs(inputsJson);
        OpenMagnetics::CoreAdviser::CoreAdviserModes coreMode;
        from_json(coreModeJson, coreMode);

        OpenMagnetics::MagneticAdviser magneticAdviser;
        magneticAdviser.set_core_mode(coreMode);

        // If user provided weights, use the weighted overload; else the unweighted one
        // (the C++ no-weights overload uses internal defaults).
        auto weights = _parse_magnetic_weights(weightsJson);
        auto masMagnetics = weights.empty()
            ? magneticAdviser.get_advised_magnetic(inputs, maximumNumberResults)
            : magneticAdviser.get_advised_magnetic(inputs, weights, maximumNumberResults);

        auto scoringsPerFilter = magneticAdviser.get_scorings();
        json results;
        results["data"] = json::array();
        for (auto& [m, s] : masMagnetics) results["data"].push_back(_build_adviser_result(m, s, &scoringsPerFilter));
        sort(results["data"].begin(), results["data"].end(),
             [](json& a, json& b) { return a["scoring"] > b["scoring"]; });
        return results;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_advised_magnetics_fast(json inputsJson, int maximumNumberResults, json coreModeJson) {
    try {
        OpenMagnetics::Inputs inputs(inputsJson);
        OpenMagnetics::CoreAdviser::CoreAdviserModes coreMode;
        from_json(coreModeJson, coreMode);

        OpenMagnetics::MagneticAdviser magneticAdviser;
        magneticAdviser.set_core_mode(coreMode);
        auto masMagnetics = magneticAdviser.get_advised_magnetic_fast(inputs, maximumNumberResults);

        // get_advised_magnetic_fast still populates per-filter scorings on the adviser,
        // so emit them too — parity with the slow path.
        auto scoringsPerFilter = magneticAdviser.get_scorings();
        json results;
        results["data"] = json::array();
        for (auto& [m, s] : masMagnetics) results["data"].push_back(_build_adviser_result(m, s, &scoringsPerFilter));
        return results;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

// RAII guard for the global coil_delimit_and_compact setting. The catalog/cache adviser
// functions require it to be true; this guard ensures it's restored on every exit path
// so we don't silently mutate the user's settings after a call (prior behaviour).
struct _CoilDelimitCompactGuard {
    bool previous;
    _CoilDelimitCompactGuard() : previous(OpenMagnetics::settings.get_coil_delimit_and_compact()) {
        OpenMagnetics::settings.set_coil_delimit_and_compact(true);
    }
    ~_CoilDelimitCompactGuard() { OpenMagnetics::settings.set_coil_delimit_and_compact(previous); }
};

json calculate_advised_magnetics_from_catalog(json inputsJson, json catalogJson, int maximumNumberResults) {
    try {
        _CoilDelimitCompactGuard _guard;
        OpenMagnetics::Inputs inputs(inputsJson);
        std::vector<OpenMagnetics::Magnetic> catalog;
        for (auto& magneticJson : catalogJson) catalog.emplace_back(magneticJson);

        OpenMagnetics::MagneticAdviser magneticAdviser;
        auto masMagnetics = magneticAdviser.get_advised_magnetic(inputs, catalog, maximumNumberResults);
        auto scoringsPerFilter = magneticAdviser.get_scorings();

        json results;
        results["data"] = json::array();
        for (auto& [m, s] : masMagnetics) results["data"].push_back(_build_adviser_result(m, s, &scoringsPerFilter));
        sort(results["data"].begin(), results["data"].end(),
             [](json& a, json& b) { return a["scoring"] > b["scoring"]; });
        return results;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

json calculate_advised_magnetics_from_cache(json inputsJson, json filterFlowJson, int maximumNumberResults) {
    try {
        _CoilDelimitCompactGuard _guard;
        OpenMagnetics::Inputs inputs(inputsJson);

        std::vector<OpenMagnetics::MagneticFilterOperation> filterFlow;
        for (auto& filterJson : filterFlowJson) filterFlow.emplace_back(filterJson);

        if (OpenMagnetics::magneticsCache.size() == 0) {
            json exception;
            exception["data"] = "Exception: No magnetics found in cache";
            return exception;
        }

        OpenMagnetics::MagneticAdviser magneticAdviser;
        auto masMagnetics = magneticAdviser.get_advised_magnetic(
            inputs, OpenMagnetics::magneticsCache.get(), filterFlow, maximumNumberResults);
        auto scoringsPerFilter = magneticAdviser.get_scorings();

        json results;
        results["data"] = json::array();
        for (auto& [m, s] : masMagnetics) results["data"].push_back(_build_adviser_result(m, s, &scoringsPerFilter));
        sort(results["data"].begin(), results["data"].end(),
             [](json& a, json& b) { return a["scoring"] > b["scoring"]; });
        return results;
    }
    catch (const std::exception &exc) {
        json exception;
        exception["data"] = "Exception: " + std::string{exc.what()};
        return exception;
    }
}

void register_adviser_bindings(py::module& m) {
    m.def("calculate_advised_cores", &calculate_advised_cores,
        R"pbdoc(
        Get recommended cores for given design requirements.
        
        Analyzes the input requirements and returns a ranked list of suitable cores
        based on the specified weights for cost, efficiency, and dimensions.
        
        Args:
            inputs_json: JSON object containing design requirements and operating points.
                         Should be processed using process_inputs() first.
            weights_json: JSON object with filter weights. Keys can be:
                         "COST", "EFFICIENCY", "DIMENSIONS" with float values 0-1.
            max_results: Maximum number of core recommendations to return.
            core_mode_json: Core selection mode - "AVAILABLE_CORES" or "STANDARD_CORES".
        
        Returns:
            JSON object with "data" array containing ranked results.
            Each result has:
            - "mas": Mas object with magnetic data
            - "scoring": Overall float score
            - "scoringPerFilter": Object with individual scores per filter
              (e.g., {"COST": 0.8, "EFFICIENCY": 0.9, "DIMENSIONS": 0.7})
        
        Example:
            >>> inputs = PyMKF.process_inputs(raw_inputs)
            >>> weights = {"COST": 1, "EFFICIENCY": 1, "DIMENSIONS": 0.5}
            >>> result = PyMKF.calculate_advised_cores(inputs, weights, 10, "AVAILABLE_CORES")
            >>> for item in result["data"]:
            ...     print(f"Score: {item['scoring']}, Per filter: {item['scoringPerFilter']}")
        )pbdoc",
        py::arg("inputs_json"), py::arg("weights_json"), 
        py::arg("max_results"), py::arg("core_mode_json"));
    
    m.def("calculate_advised_magnetics", &calculate_advised_magnetics,
        R"pbdoc(
        Get recommended complete magnetic designs for given requirements.

        Performs full magnetic design optimization including core selection,
        winding configuration, and all parameters. Returns complete Mas
        (Magnetic Assembly Specification) objects ready for manufacturing.

        Args:
            inputs_json: Design requirements + operating points (use process_inputs() first).
            max_results: Maximum number of magnetic recommendations to return.
            core_mode_json: "available cores" (commercial) | "standard cores" (faster).
            weights_json: Optional weights dict {"cost": w, "dimensions": w, "efficiency": w,
                          "losses": w, ...}. Keys are MagneticFilters enum values (case-sensitive).
                          Pass None or {} for the C++ default ranking.

        Returns:
            JSON object with "data" array containing ranked results.
            Each result has:
            - "mas": Mas object with magnetic, inputs, and optionally outputs
            - "scoring": Overall float score
            - "scoringPerFilter": Object with individual scores per filter
              (e.g., {"cost": 0.8, "losses": 0.9, "dimensions": 0.7})

        Example:
            >>> inputs = PyMKF.process_inputs(raw_inputs)
            >>> result = PyMKF.calculate_advised_magnetics(inputs, 5, "available cores",
            ...     {"efficiency": 2.0, "cost": 1.0, "dimensions": 0.5})
            >>> for item in result["data"]:
            ...     print(f"Score: {item['scoring']}, Per filter: {item['scoringPerFilter']}")
        )pbdoc",
        py::arg("inputs_json"), py::arg("max_results"), py::arg("core_mode_json"),
        py::arg("weights_json") = json());
    
    m.def("calculate_advised_magnetics_fast", &calculate_advised_magnetics_fast,
        R"pbdoc(
        Get recommended complete magnetic designs using fast analytical mode.

        Performs rapid magnetic design exploration using analytical turn/gap
        calculation and simplified loss evaluation (DC ohmic + core losses only).
        Bypasses CoilAdviser optimization and full MagneticSimulator for speed.
        Results are sorted by ascending total losses (lower losses = better rank).

        Suitable for design space exploration and Pareto front generation.
        For production designs use calculate_advised_magnetics() instead.

        Args:
            inputs_json: JSON object containing design requirements and operating points.
                         Should be processed using process_inputs() first.
            max_results: Maximum number of magnetic recommendations to return.
            core_mode_json: Core selection mode - "AVAILABLE_CORES" or "STANDARD_CORES".

        Returns:
            JSON object with "data" array containing results sorted by total losses.
            Each result has:
            - "mas": Mas object with magnetic, inputs, and outputs (losses data)
            - "scoring": Total losses value in watts (lower is better)

        Example:
            >>> inputs = PyMKF.process_inputs(raw_inputs)
            >>> result = PyMKF.calculate_advised_magnetics_fast(inputs, 5, "STANDARD_CORES")
            >>> for item in result["data"]:
            ...     print(f"Total losses: {item['scoring']} W")
        )pbdoc",
        py::arg("inputs_json"), py::arg("max_results"), py::arg("core_mode_json"));

    m.def("calculate_advised_magnetics_from_catalog", &calculate_advised_magnetics_from_catalog,
        R"pbdoc(
        Get recommended magnetics from a custom component catalog.
        
        Evaluates magnetic components from a user-provided catalog against
        the design requirements and returns ranked recommendations.
        
        Args:
            inputs_json: JSON object containing design requirements and operating points.
            catalog_json: JSON array of Magnetic objects to evaluate.
            max_results: Maximum number of recommendations to return.
        
        Returns:
            JSON object with "data" array containing ranked results.
            Each result has:
            - "mas": Mas object with magnetic data
            - "scoring": Overall float score
            - "scoringPerFilter": Object with individual scores per filter
        
        Example:
            >>> inputs = PyMKF.process_inputs(raw_inputs)
            >>> catalog = [magnetic1, magnetic2, magnetic3]
            >>> result = PyMKF.calculate_advised_magnetics_from_catalog(inputs, catalog, 5)
            >>> for item in result["data"]:
            ...     print(f"Score: {item['scoring']}, Per filter: {item['scoringPerFilter']}")
        )pbdoc",
        py::arg("inputs_json"), py::arg("catalog_json"), py::arg("max_results"));
    
    m.def("calculate_advised_magnetics_from_cache", &calculate_advised_magnetics_from_cache,
        R"pbdoc(
        Get recommended magnetics from previously cached designs.
        
        Evaluates cached magnetic designs against the requirements using
        a custom filter flow for advanced filtering operations.
        
        Args:
            inputs_json: JSON object containing design requirements and operating points.
            filter_flow_json: JSON array of MagneticFilterOperation objects defining
                              the filtering pipeline.
            max_results: Maximum number of recommendations to return.
        
        Returns:
            JSON object with "data" array containing ranked results,
            or error string if cache is empty.
            Each result has:
            - "mas": Mas object with magnetic data
            - "scoring": Overall float score
            - "scoringPerFilter": Object with individual scores per filter
        
        Note:
            Cache must be populated before calling this function.
            Returns "Exception: No magnetics found in cache" if cache is empty.
        )pbdoc",
        py::arg("inputs_json"), py::arg("filter_flow_json"), py::arg("max_results"));
}

} // namespace PyMKF
