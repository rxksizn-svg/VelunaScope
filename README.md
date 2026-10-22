# Veluna Scope 0.1

Windows x64 VST3 loudness meter SOURCE PROTOTYPE. Not compiled or tested in this session. See คู่มือภาษาไทย.md for usage and SOURCES.md for reference provenance.

Mono/stereo/5.1/7.1/7.1.4 PCM. M/S/I/LRA/true peak via libebur128. VAD-selected integrated is an approximation, not Dolby dialogue-gated measurement. No Atmos object decoder or delivery certification.

Build commands:
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure

Online: .github/workflows/scope-windows.yml
Use a separate repository from Bloom/Halo. Workflow packages only after tests pass.
