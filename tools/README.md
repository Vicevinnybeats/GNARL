# tools

Code generators for the parameter layer. They are **not** part of the CMake
build: they are run by hand and their output is committed, so the generated
files stay plain, greppable source rather than build artefacts.

```bash
python3 tools/generate_parameter_ids.py      # -> plugin/source/params/ParameterIDs.h
python3 tools/generate_parameter_mirror.py   # -> ui/src/bridge/parameterIds.ts
```

Run them in that order: the mirror is derived from the C++ header, which is
what makes the two impossible to disagree by hand.

To add a parameter, edit the field lists at the top of
`generate_parameter_ids.py`, run both generators, then add the parameter to
`plugin/source/params/ParameterLayout.cpp` (which is hand-written, because each
parameter's range, skew and default is a decision that wants a comment) and
bump `params::getDeclaredParameterCount()`.

`tests/ParameterMirrorTests.cpp` and `tests/ParameterLayoutTests.cpp` fail the
build if any of those steps is skipped.
