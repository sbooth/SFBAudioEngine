# Agent Guidelines for SFBAudioEngine

## Repository Overview

SFBAudioEngine is a Swift/Objective-C/C++ audio library for macOS 11+, iOS 15+, and tvOS 15+. It supports audio decoding, playback, encoding, format conversion, and metadata editing. The library is distributed as a Swift Package.

## Repository Structure

```
Sources/
  CSFBAudioEngine/          # C/Objective-C/C++ core (the main implementation)
    include/SFBAudioEngine/ # Public headers
    Decoders/               # Audio decoders (PCM and DSD)
    Encoders/               # Audio encoders
    Player/                 # SFBAudioPlayer implementation
    Input/                  # SFBInputSource (file, buffer, data)
    Output/                 # SFBOutputTarget (file, buffer, data)
    Conversion/             # SFBAudioConverter
    Metadata/               # SFBAudioFile, SFBAudioProperties, SFBAudioMetadata
    Analysis/               # Audio analysis utilities
    Utilities/              # Shared internal utilities
  SFBAudioEngine/           # Swift wrappers and extensions
Tests/
  SFBAudioEngineTests/      # Swift tests (SFBAudioEngineTests.swift)
```

## Build and Test

This project uses Swift Package Manager.

```bash
# Build
swift build

# Run tests
swift test
```

> **Note:** Building requires macOS with Xcode installed (latest stable). The package links against system Apple frameworks (AudioToolbox, AVFAudio, Accelerate, Foundation, ImageIO, UniformTypeIdentifiers) and several binary XCFramework dependencies that are fetched automatically by SPM.

CI runs `swift build -v` and `swift test -v` on `macos-latest` for every push and pull request to `main`.

## Code Style

### C/Objective-C/C++ (`CSFBAudioEngine`)
- Formatting is enforced by `.clang-format` (based on LLVM style).
- Column limit: 120. Indent: 4 spaces (no tabs).
- Run `clang-format` before committing C/ObjC/C++ changes.
- A `cpp-linter` CI workflow also checks formatting automatically.

### Swift (`SFBAudioEngine`)
- Follow the conventions of the existing Swift files in `Sources/SFBAudioEngine/`.
- No separate Swift linter is configured; match the style of surrounding code.

### General
- `.editorconfig` is present — editors that support it will pick up baseline settings automatically.

## Language Standards

- C: C11 (`c11`)
- C++: C++20 (`cxx20`)
- Swift: current toolchain default

## Key Design Patterns

- **Decoders** implement `SFBAudioDecoding`. PCM decoders additionally implement `SFBPCMDecoding`; DSD decoders implement `SFBDSDDecoding`.
- **Encoders** implement `SFBAudioEncoding` (PCM-consuming encoders also implement `SFBPCMEncoding`).
- **Input/Output** abstraction uses `SFBInputSource` and `SFBOutputTarget`, which can wrap files, buffers, or `Data` objects.
- **Metadata** is accessed via `SFBAudioFile`, which exposes `SFBAudioProperties` (read-only) and `SFBAudioMetadata` (read-write for most formats).
- **Playback** is provided by `SFBAudioPlayer`, built on `AVAudioEngine` and `AVAudioSourceNode`.

## Adding a New Decoder or Encoder

1. Add the implementation files under `Sources/CSFBAudioEngine/Decoders/` or `Sources/CSFBAudioEngine/Encoders/`.
2. Implement the appropriate protocol (`SFBPCMDecoding`, `SFBDSDDecoding`, or `SFBPCMEncoding`).
3. Register the new class so it is discovered by the factory methods (follow existing decoders/encoders as a pattern).
4. If a new third-party library dependency is required, add it to `Package.swift` and check that its license is compatible with MIT.

## Dependencies

Third-party libraries are pulled in via SPM package dependencies declared in `Package.swift`. Most audio format libraries (FLAC, Ogg, Opus, Vorbis, WavPack, libsndfile, mpg123, LAME, Musepack, TTA, libspeex, DUMB, Monkey's Audio, TagLib) are provided as pre-built binary XCFrameworks or source packages under the `sbooth` GitHub organization.

> **LGPL note:** libsndfile, mpg123, libtta-cpp, LAME, and the Musepack encoder are LGPL. They are dynamically linked to maintain MIT compatibility. Do not statically link these libraries.

## Pull Requests

- Target the `main` branch.
- Ensure `swift build` and `swift test` pass locally before opening a PR.
- C/ObjC/C++ files should be formatted with `clang-format` prior to submission.
