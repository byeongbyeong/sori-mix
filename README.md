# SoriMix

SoriMix is an AI-assisted mixing plugin built with JUCE.

This project starts from the broad idea of a prompt-driven mix assistant, but it is implemented as a clean new codebase:

- real-time safe DSP core
- AudioProcessorValueTreeState parameters
- simple musical controls for tone, compression, width, and output
- command presets that can later be replaced by a secure AI service layer
- CMake-based local and GitHub Actions builds

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The first configure downloads JUCE 8.0.4 through CMake FetchContent.

## Roadmap

- Add a secure AI provider abstraction.
- Store API credentials in the OS keychain instead of plaintext files.
- Add a spectrum/meter panel.
- Add preset import/export.
- Add notarized release packaging.
