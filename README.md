# Quick Pitch Shifter

A compact live pitch-shifting plugin by **Nytera**, using [Signalsmith Stretch](https://signalsmith-audio.co.uk/code/stretch/). Version 0.1.0 supports mono and stereo and preserves the timing of incoming audio.

## Controls

| Control | Range | Default | Purpose |
| --- | --- | --- | --- |
| Pitch | -12 to +12 semitones | 0 | Drag in whole semitones, or type fractional semitones in the textbox. |
| Tonality limit | 2,000–20,000 Hz | 8,000 Hz | Change high-frequency pitch mapping to retain timbre; this is not a filter cutoff. |
| Preserve formants | Off/on | Off | Retain more of the source's vocal or instrumental character during pitch shifting. |
| Formant base | 50–500 Hz | 200 Hz | Expected fundamental used for formant analysis; enabled when formant preservation is on. |
| Analysis block | 50–180 ms | 120 ms | Trade time/frequency resolution and quality against latency. |
| Overlap | 2–8 | 4 | Analysis block divided by processing interval; larger values generally cost more CPU. |
| Bypass | Off/on | Off | Crossfade to delayed, unshifted audio. |

Double-click a slider to restore its default. Numeric values can be edited directly. Pitch, tonality, formants, and bypass are host parameters saved with the plugin state. Pitch and frequency controls are smoothed over 20 ms; bypass crossfades over 10 ms.

Analysis block and overlap are setup controls: edits remain pending until **Apply**. Applying rebuilds the processor on a worker thread, can briefly interrupt audio, and updates the latency reported to the host. The applied configuration is saved with plugin state; unapplied editor changes are discarded when the editor closes. These two settings are deliberately not host automation parameters.

## Latency

The demo's split computation is enabled. The default analysis block of 120 ms and overlap of 4 add approximately **150 ms** of effect latency. The shortest available configuration (50 ms / overlap 8) adds approximately **56.25 ms**. The displayed latency is queried from the processor in samples and excludes host/device buffering.

This version targets the broadcast path. Comfortable monitoring through the effect is not a design requirement. Bypass retains processing latency and keeps the pitch engine running. Zero semitones with bypass off still uses the spectral processor.

## Build on Windows x64

Requirements: CMake 3.24 or later, Git, Visual Studio 2022 C++ Build Tools, and a Windows SDK. The first configure requires network access to fetch the pinned dependencies; their source is cached under `build/_deps`.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DQPS_BUILD_TESTS=ON
cmake --build build --config Release --parallel 4 --target QuickPitchShifter_VST3 QuickPitchShifter_CLAP QuickPitchShifterTests
ctest --test-dir build -C Release --output-on-failure
./scripts/package-windows.ps1
```

VST3 output: `build/QuickPitchShifter_artefacts/Release/VST3/Quick Pitch Shifter.vst3`.

CLAP output: `build/QuickPitchShifter_artefacts/Release/CLAP/Quick Pitch Shifter.clap`.

Copy the **entire VST3 bundle directory** to a VST3 directory scanned by your host. The build does not install plugins automatically. Packaging produces `dist/QuickPitchShifter-windows-x64.zip`, containing binaries, project source, and license notices.

To render a preview of the actual JUCE editor while running the verification suite:

```powershell
./build/QuickPitchShifterTests_artefacts/Release/QuickPitchShifterTests.exe --snapshot "$PWD/build/editor-preview.png"
```

## Cross-platform workflow

`.github/workflows/release.yml` follows [ForceSimpleDelay](https://github.com/reon-nynes/ForceSimpleDelay)'s build and release structure:

- Windows x64: VST3 and CLAP; runs the processor verification suite.
- Linux x64: VST3 and CLAP.
- macOS universal (Intel and Apple Silicon): VST3, CLAP, and AUv3 embedded in its required containing app.

The Windows workflow also validates the built VST3 with pluginval 1.0.4 at strictness level 5, with GUI-window tests skipped. The processor suite separately exercises editor creation and pending/Apply behavior. To run the same VST3 checks locally after downloading [pluginval](https://github.com/Tracktion/pluginval/releases/tag/v1.0.4):

```powershell
./scripts/validate-windows.ps1 -PluginvalPath ./build/tools/pluginval/pluginval.exe
```

Run the workflow manually for downloadable artifacts. Pushing a `v*` tag builds all platforms and publishes their archives as a GitHub release. macOS packages are unsigned; signing and notarization are not configured. The macOS containing app exists for AUv3 packaging; routing and hosting features are outside this project.

Local non-Windows configuration examples:

```sh
# Linux, after installing the packages listed in the workflow
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4 --target QuickPitchShifter_VST3 QuickPitchShifter_CLAP

# macOS
cmake -S . -B build -G Xcode "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build --config Release --parallel 4 --target QuickPitchShifter_VST3 QuickPitchShifter_CLAP QuickPitchShifter_Standalone
```

## Source layout

- `src/PitchEngine.*`: duration-preserving Signalsmith processing, preallocated scratch buffers, smoothing, and aligned dry bypass.
- `src/PluginProcessor.*`: JUCE buses, parameters, state, latency reporting, and worker-thread configuration.
- `src/PluginEditor.*`: compact custom JUCE interface and pending processing settings.
- `tests/ProcessorTests.cpp`: pitch-frequency checks, stereo coherence, callback allocation checks, bypass impulse delay, state restoration, live controls, and reconfiguration.
- `docs/design.md`, `docs/adr/`, and `GLOSSARY.md`: agreed design and domain terminology.

## License and dependencies

Project-owned code is licensed under **GPL-3.0**; see `LICENSE`. JUCE 8 modules are separately licensed under **AGPL-3.0 or a commercial JUCE license**; distributing binaries containing those modules must satisfy the applicable JUCE terms as well. Third-party license texts and pinned source references are in `licenses/` and `THIRD_PARTY_NOTICES.md`. The implementation is new; the ForceSimpleDelay repository serves as a CMake/JUCE and workflow reference.
