# Sori 1

Sori 1 is an AI-assisted vocal chain plugin by Sori LAB. It is built with JUCE and is currently implemented as the `SoriMix` plugin target while the product moves toward a commercial release.

Website: https://byeongbyeong.github.io/sori-mix/

Repository: https://github.com/byeongbyeong/sori-mix

## Current Status

The project is a working macOS JUCE plugin prototype focused on vocal mixing. It builds AU, VST3, and Standalone formats, includes a stage-based vocal chain, supports secure API key storage on macOS, and has a first AI assistant layer that can request parameter plans from OpenAI or Groq.

The current product scope intentionally excludes pitch/modulation correction and auto-tune. Those will be handled separately. Sori 1 focuses on the post-tuning vocal chain:

```text
DeEss > Res EQ > Comp > EQ > Sat > Inflate
```

## Product Direction

Sori 1 is designed for vocal production rather than general-purpose mixing. The goal is to give the user a clear modular chain where each stage has a focused role, can be enabled or bypassed independently, and can be reordered when the user wants a different workflow.

The plugin is being developed around three core ideas:

- Role-based vocal processing instead of a generic knob collection.
- Stage-aware AI assistance that maps a prompt to the correct part of the chain.
- Practical monitoring: input level, output level, gain reduction, whole-chain before/after comparison, and module on/off state.

## Plugin Formats

The CMake target currently builds:

- AU component
- VST3 plugin
- Standalone app

The internal JUCE target and current build artifact name are still `SoriMix`. The public-facing brand is `Sori LAB`, and the product name is `Sori 1`.

## Module Overview

| Stage | Code module | Main purpose | Current control mapping |
| --- | --- | --- | --- |
| DeEss | `DeEsserModule` | Controls sibilance and harsh consonants while preserving vocal air. | `highGain` is currently reused as de-essing intensity. |
| Res EQ | `ResonanceEqModule` | Suppresses narrow resonant buildup around the selected focus area. | Negative `midGain` controls reduction amount, `midFreq` controls target area. |
| Comp | `GlueModule` | Vocal-first leveling, peak control, density, and frontness. | `compAmount` controls compression intensity. |
| EQ | `ToneModule` | Musical tone shaping for body, focus, presence, and air. | `lowGain`, `midGain`, `midFreq`, `highGain`. |
| Sat | `SaturationModule` | Adds harmonic density, warmth, and controlled edge. | `mix` is currently reused as saturation density. |
| Inflate | `WidthModule` plus output gain | Adds perceived size, width, and final output control. | `width`, `outputGain`. |

Some controls are currently shared between modules for development speed. A later product pass should split these into dedicated public parameters per module so the UI, automation names, presets, and AI plans are cleaner.

## Vocal Chain Architecture

The chain structure is defined by:

- `Source/VocalChain.h`
- `Source/VocalChain.cpp`
- `Source/ChainEngine.h`
- `Source/ChainEngine.cpp`

`VocalChain` defines the six available stages, short names, display names, and assistant context text. `ChainEngine` converts the six chain slot choices into a valid processing order.

Duplicate chain slot selections are automatically resolved. If the same module is selected more than once, the engine keeps the first occurrence and fills the remaining slots with missing stages in the default order.

Default order:

```text
DeEss > Res EQ > Comp > EQ > Sat > Inflate
```

## DSP Flow

The audio processor is implemented in:

- `Source/PluginProcessor.h`
- `Source/PluginProcessor.cpp`
- `Source/DSPModules.h`
- `Source/DSPModules.cpp`

The processing flow is:

1. Read input level for metering.
2. Update module parameters from `AudioProcessorValueTreeState`.
3. Read current chain slot order.
4. Smooth module enable/bypass transitions.
5. Process each active stage in chain order.
6. Apply output gain.
7. Apply global mix and before/after compare blend.
8. Publish output level and gain reduction meter values.

Each module uses a dry copy when stage wet/bypass smoothing is needed. This allows a module to fade in/out without hard audio discontinuities.

## Stage Bypass and CPU Behavior

Each stage has its own enable parameter:

- `deEsserEnabled`
- `resonanceEqEnabled`
- `compressorEnabled`
- `musicalEqEnabled`
- `saturationEnabled`
- `inflatorEnabled`

When a stage is off and no smoothing is active, the processor avoids running the expensive module path where possible. Stateful modules such as the de-esser, compressor, saturation, and width stage still receive skip calls so their smoothing state stays coherent.

This is important for vocal sessions where a user may only need one module on a given track, for example using only DeEss or only EQ.

## Parameters

Current public parameters:

| Parameter ID | Display name | Range | Role |
| --- | --- | --- | --- |
| `lowGain` | Low Gain | -12 dB to +12 dB | Low/body tone shaping. |
| `midGain` | Mid Gain | -12 dB to +12 dB | Mid tone shaping and resonance reduction amount when negative. |
| `midFreq` | Mid Focus | 250 Hz to 4500 Hz | Mid EQ focus and resonance target area. |
| `highGain` | High Gain | -12 dB to +12 dB | High tone shaping and temporary de-essing intensity source. |
| `compAmount` | Glue | 0 to 1 | Compressor amount. |
| `width` | Width | 0 to 2 | Inflator/width amount. |
| `outputGain` | Output | -24 dB to +12 dB | Final gain. |
| `mix` | Mix | 0 to 1 | Global wet/dry mix and temporary saturation density source. |
| `compareBefore` | Compare Before | on/off | Whole-chain before/after comparison. |

Chain order parameters:

- `stageSlot1`
- `stageSlot2`
- `stageSlot3`
- `stageSlot4`
- `stageSlot5`
- `stageSlot6`

## UI Structure

The editor is implemented in:

- `Source/PluginEditor.h`
- `Source/PluginEditor.cpp`

The UI is organized around the selected vocal stage. It includes:

- Stage buttons for DeEss, Res EQ, Comp, EQ, Sat, and Inflate.
- Stage on/off control.
- Whole-chain before/after compare.
- Chain slot controls for custom ordering.
- Prompt box and quick command buttons.
- OpenAI/Groq provider selector.
- API key save/delete controls.
- Input, output, and gain reduction meters.

The design direction is a clean, stage-based vocal workflow rather than one dense all-in-one control panel.

## AI Assistant Layer

The assistant layer is implemented in:

- `Source/AssistantClient.h`
- `Source/AssistantClient.cpp`
- `Source/AssistantTypes.h`

The current assistant flow:

1. User selects a provider: OpenAI or Groq.
2. User saves an API key.
3. User enters a mix request.
4. The assistant sends a chat completion request.
5. The model is instructed to return a JSON-only parameter plan.
6. The plugin parses and clamps the returned values.
7. Valid parameter changes are applied to the plugin state.

Current default provider models:

- OpenAI: `gpt-4o-mini`
- Groq: `llama-3.1-8b-instant`

The assistant system prompt is stage-aware. It explains the preferred vocal chain and maps each stage to the current parameter surface. For example, a DeEss prompt should mostly affect sibilance control, while a Compressor prompt should mostly affect `compAmount`.

Current supported assistant plan fields:

- `lowGain`
- `midGain`
- `midFreq`
- `highGain`
- `compAmount`
- `width`
- `outputGain`
- `mix`
- `toneEnabled`
- `glueEnabled`
- `widthEnabled`
- `summary`

The AI layer is functional but still early. A commercial version should add richer stage-specific schemas, better error recovery, model selection policy, request cancellation, and more explicit user confirmation for larger parameter changes.

## Secure API Key Storage

API key storage is implemented in:

- `Source/SecureCredentialStore.h`
- `Source/SecureCredentialStore.cpp`
- `Source/SecureCredentialStore_mac.mm`
- `Source/SecureCredentialStore_fallback.cpp`

On macOS, API keys are stored in Keychain using the service name:

```text
com.byeongbyeong.sorimix.ai
```

The provider name is used as the Keychain account. Non-Apple platforms currently return `unavailable` until secure storage is implemented for those platforms.

API keys are not stored in project files.

## Website

The product website is in:

- `docs/index.html`
- `docs/styles.css`
- `docs/assets/`

Live site:

```text
https://byeongbyeong.github.io/sori-mix/
```

The current website presents the product as `Sori LAB - Sori 1`, includes a product UI preview, explains the six-stage vocal chain, and links to GitHub Actions and Releases.

GitHub Pages deployment is configured in:

```text
.github/workflows/deploy-pages.yml
```

## Build

Requirements:

- macOS for AU validation and Keychain support
- CMake 3.22+
- Xcode command line tools
- Internet access on first configure so CMake can fetch JUCE 8.0.4

Configure:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Build:

```bash
cmake --build build --config Release --parallel
```

Universal macOS CI builds use:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --parallel
```

## Validation

Local validation script:

```bash
scripts/validate-plugin.sh
```

The script:

1. Builds the current configured build directory.
2. Runs pluginval against the VST3 artifact.
3. Copies the AU component into the user AU plugin folder.
4. Runs `auval`.

Defaults:

- Build config: `Debug`
- pluginval path: `/Applications/pluginval.app/Contents/MacOS/pluginval`
- pluginval strictness: `5`
- pluginval seed: `23063`

Override examples:

```bash
BUILD_CONFIG=Release scripts/validate-plugin.sh
PLUGINVAL=/path/to/pluginval scripts/validate-plugin.sh
```

## CI

macOS plugin builds are configured in:

```text
.github/workflows/build-mac.yml
```

The workflow builds AU, VST3, and Standalone artifacts and uploads them as `SoriMix-mac`.

GitHub Pages deployment is configured separately in:

```text
.github/workflows/deploy-pages.yml
```

## Repository Structure

```text
Source/
  AssistantClient.*            AI provider request and JSON plan parsing.
  AssistantTypes.h             Assistant plan data structure.
  ChainEngine.*                Chain slot normalization and order creation.
  DSPModules.*                 De-esser, resonance EQ, compressor, EQ, saturation, width modules.
  PluginEditor.*               Stage-based JUCE UI.
  PluginProcessor.*            Audio processor, parameters, chain execution, state save/load.
  SecureCredentialStore.*      API key storage abstraction and platform implementations.
  VocalChain.*                 Stage definitions and display metadata.

docs/
  index.html                   Product website.
  styles.css                   Website styling.
  assets/                      Favicon and product image assets.

scripts/
  validate-plugin.sh           Local pluginval and auval validation script.

.github/workflows/
  build-mac.yml                macOS plugin build workflow.
  deploy-pages.yml             GitHub Pages deployment workflow.
```

## Important Implementation Notes

- The current public plugin target is still named `SoriMix`; product naming is moving toward `Sori 1`.
- Some stage controls are temporarily mapped to shared parameters. This should be split before final product release.
- Pitch/modulation and auto-tune are intentionally out of scope for this plugin.
- The assistant currently applies direct parameter changes; future versions should support richer plans, preview/confirm behavior, and better stage-specific control names.
- The current code is a development prototype, not a notarized commercial installer.

## Roadmap

High-priority next steps:

1. Rename product-facing JUCE metadata from `SoriMix` to `Sori 1` when ready.
2. Split shared parameters into dedicated module parameters.
3. Expand stage-specific UI controls for DeEss, Res EQ, Comp, EQ, Sat, and Inflate.
4. Improve metering with per-stage reduction/activity feedback.
5. Add preset save/load and factory vocal presets.
6. Add assistant preview/confirm flow before applying larger changes.
7. Add notarized macOS packaging and signed releases.
8. Prepare commercial release documentation, changelog, license, and installer flow.
