# Fix: `export_magnetic_as_subcircuit` — TypeError on return

## The symptom

Calling `export_magnetic_as_subcircuit` from Python always raised:

```
TypeError: Unable to convert function return value to a Python type!
(arg0: json) -> nlohmann::json_abi_v3_11_3::basic_json<nlohmann::ordered_map, ...>
```

The function was completely unusable from Python.

---

## Root cause

`simulation.cpp` stored the result in the wrong type before returning it to pybind11.

The C++ library function (`OpenMagnetics::CircuitSimulatorExporter::export_magnetic_as_subcircuit`)
already returns `std::string` — the SPICE subcircuit text.

The wrapper in `simulation.cpp` was incorrectly storing that string in `ordered_json`
(= `nlohmann::ordered_json`, a different template instantiation of `basic_json` than the regular `json`).
It then called `.dump(4)` on it, which re-serialised the already-plain string into a JSON-quoted string,
and returned that wrapped inside `ordered_json`.

pybind11 has converters registered for `nlohmann::json` (the standard alias) but **not** for
`nlohmann::ordered_json`. Since the return type is a different type, pybind11 cannot convert it
to a Python object, and raises the `TypeError`.

---

## The fix

**`src/simulation.h` — line 11**

```diff
- ordered_json export_magnetic_as_subcircuit(json magneticJson);
+ std::string   export_magnetic_as_subcircuit(json magneticJson);
```

**`src/simulation.cpp` — lines 40–50**

```diff
- ordered_json export_magnetic_as_subcircuit(json magneticJson) {
+ std::string export_magnetic_as_subcircuit(json magneticJson) {
      try {
          OpenMagnetics::Magnetic magnetic(magneticJson);
-         ordered_json subcircuit = OpenMagnetics::CircuitSimulatorExporter().export_magnetic_as_subcircuit(magnetic);
-         return subcircuit.dump(4);
+         return OpenMagnetics::CircuitSimulatorExporter().export_magnetic_as_subcircuit(magnetic);
      }
      catch (const std::exception &exc) {
-         ordered_json exception;
-         exception["data"] = "Exception: " + std::string{exc.what()};
-         return exception;
+         return "Exception: " + std::string{exc.what()};
      }
  }
```

The wrapper now returns `std::string` directly. pybind11 converts this to a Python `str`
with no issues.

---

## What the function returns after the fix

A plain Python string containing the SPICE subcircuit in NgSpice format, using the
**LADDER mode** by default (physics-based R-L ladder for skin/proximity effect + core losses).

```python
spice_text = PyOpenMagnetics.export_magnetic_as_subcircuit(mag)
# spice_text is a str — save directly to a .sp file
```

The mode can be changed before calling via:
```python
PyOpenMagnetics.set_settings({"circuitSimulatorCurveFittingMode": "FRACPOLE"})
# options: "ANALYTICAL", "LADDER", "FRACPOLE", "ROSANO", "ROSANO_RLC", "AUTO"
```

---

## Is this worth a PR to the original repo?

Yes — the function is completely broken for all Python users on all platforms until this fix.
The change is 2 lines deleted, 2 lines changed, zero behaviour change in the C++ logic.
