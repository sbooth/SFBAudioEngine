# SFBAudioEngine

[![](https://img.shields.io/endpoint?url=https%3A%2F%2Fswiftpackageindex.com%2Fapi%2Fpackages%2Fsbooth%2FSFBAudioEngine%2Fbadge%3Ftype%3Dswift-versions)](https://swiftpackageindex.com/sbooth/SFBAudioEngine)
[![](https://img.shields.io/endpoint?url=https%3A%2F%2Fswiftpackageindex.com%2Fapi%2Fpackages%2Fsbooth%2FSFBAudioEngine%2Fbadge%3Ftype%3Dplatforms)](https://swiftpackageindex.com/sbooth/SFBAudioEngine)

SFBAudioEngine is a powerhouse of audio functionality for macOS, iOS, and tvOS. SFBAudioEngine supports:

* [Audio decoding](#decoding)
* [Audio playback](#playback)
* [Audio encoding](#encoding)
* [Audio format conversion](#conversion)
* [Audio properties information and metadata editing](#properties-and-metadata)

SFBAudioEngine is usable from both Swift and Objective-C.

## Format Support

SFBAudioEngine supports most audio formats. In addition to all formats supported by [Core Audio](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html) SFBAudioEngine supports:

* [Ogg Speex](https://www.speex.org)
* [Ogg Vorbis](https://xiph.org/vorbis/)
* [Monkey's Audio](https://www.monkeysaudio.com)
* [Musepack](https://www.musepack.net)
* Shorten
* True Audio
* [WavPack](http://www.wavpack.com)
* All formats supported by [libsndfile](http://libsndfile.github.io/libsndfile/)
* DSD to PCM conversion for DSD64
* DSD decoding for DSF and DSDIFF with support for DSD over PCM (DoP)

[FLAC](https://xiph.org/flac/), [Ogg Opus](https://opus-codec.org), and MP3 are natively supported by Core Audio, however SFBAudioEngine provides its own encoders and decoders for these formats.

## Quick Start

### Playback

Playing an audio file is as simple as:

~~~swift
import SFBAudioEngine
let player = AudioPlayer()
let url = URL(fileURLWithPath: "example.flac")
try? player.play(url)
~~~

> [!NOTE]
> Only file URLs are supported.

### Metadata

Reading audio properties and metadata is similarly trivial:

~~~swift
if let audioFile = try? AudioFile(readingPropertiesAndMetadataFrom: url) {
    let sampleRate = audioFile.properties.sampleRate
    let title = audioFile.metadata.title
}
~~~

### Conversion

Want to convert a WAVE file to FLAC?

~~~swift
let inputURL = URL(fileURLWithPath: "music.wav")
let outputURL = URL(fileURLWithPath: "music.flac")
try AudioConverter.convert(inputURL, to: outputURL)
~~~

The output file's format is inferred from the file extension.

More complex conversions are supported including writing to `Data` instead of files:

~~~swift
let output = OutputTarget.makeForData()
let encoder = try AudioEncoder(outputTarget: output, encoderName: .coreAudio)
encoder.settings = [
    .coreAudioFileTypeID: kAudioFileM4AType,
    .coreAudioFormatID: kAudioFormatMPEG4AAC,
    .coreAudioAudioConverterPropertySettings: [kAudioConverterCodecQuality: kAudioConverterQuality_High]
]
try AudioConverter.convert(inputURL, using: encoder)
// Encoder output is in `output.data`
~~~

## Requirements

macOS 11.0+, iOS 15.0+, or tvOS 15.0+

## Installation

### Swift Package Manager

Add a package dependency to https://github.com/sbooth/SFBAudioEngine in Xcode.

### Manual or Custom Build

1. Clone the [SFBAudioEngine](https://github.com/sbooth/SFBAudioEngine) repository.
2. `swift build`.

## Decoding

[Audio decoders](Sources/CSFBAudioEngine/Decoders/) in SFBAudioEngine are broadly divided into two categories, those producing PCM output and those producing DSD output. Audio decoders read data from an [SFBInputSource](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBInputSource.h) which may refer to a file, buffer, or data.

All audio decoders in SFBAudioEngine implement the [SFBAudioDecoding](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioDecoding.h) protocol. PCM-producing decoders additionally implement [SFBPCMDecoding](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBPCMDecoding.h) while DSD decoders implement [SFBDSDDecoding](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBDSDDecoding.h).

Three special decoder subclasses that decorate an underlying audio decoder instance are also provided: [SFBAudioRegionDecoder](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioRegionDecoder.h), [SFBDoPDecoder](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBDoPDecoder.h), and [SFBDSDPCMDecoder](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBDSDPCMDecoder.h). For seekable inputs, [SFBAudioRegionDecoder](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioRegionDecoder.h) allows playback and looping of a PCM decoder region. [SFBDoPDecoder](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBDoPDecoder.h) and [SFBDSDPCMDecoder](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBDSDPCMDecoder.h) decorate a DSD decoder providing DSD over PCM (DoP) and PCM output respectively.

## Playback

### [SFBAudioPlayer](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioPlayer.h)

[SFBAudioPlayer](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioPlayer.h) uses an [AVAudioEngine](https://developer.apple.com/documentation/avfaudio/avaudioengine) processing graph driven by [AVAudioSourceNode](https://developer.apple.com/documentation/avfaudio/avaudiosourcenode) for playback. [SFBAudioPlayer](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioPlayer.h) provides complete player functionality with no required configuration but also allows customization of the underlying processing graph as well as rich status notifications through a delegate.

## Encoding

[Audio encoders](Sources/CSFBAudioEngine/SFBAudioEngine/Encoders/) in SFBAudioEngine process input data and convert it to their output format. Audio encoders write data to an [SFBOutputTarget](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBOutputTarget.h) which may refer to a file, buffer, or data.

All audio encoders in SFBAudioEngine implement the [SFBAudioEncoding](Sources/CSFBAudioEngine/include/SFBAudioEncoding.h) protocol. PCM-consuming encoders additionally implement [SFBPCMEncoding](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBPCMEncoding.h). Currently there are no encoders consuming DSD in SFBAudioEngine.

Encoders don't support arbitrary input formats. The processing format used by an encoder is derived from a desired format combined with the encoder's settings.

## Conversion

[SFBAudioConverter](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioConverter.h) supports high level conversion operations. An audio converter reads PCM audio from an audio decoder in the decoder's processing format, converts that audio to an intermediate PCM format, and then writes the intermediate PCM audio to an audio encoder which performs the final conversion to the desired format.

The decoder's processing format and the intermediate format must both be PCM but do not have to have the same sample rate, bit depth, channel count, or channel layout.

## Properties and Metadata

Audio properties and metadata are accessed via instances of [SFBAudioFile](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioFile.h). [Audio properties](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioProperties.h) are read-only while [metadata](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioMetadata.h) is writable for most formats. Audio metadata may be obtained from an instance of [SFBAudioFile](Sources/CSFBAudioEngine/include/SFBAudioEngine/SFBAudioFile.h) or instantiated directly. 

## Sample Audio Players

Two versions of SimplePlayer, one for macOS and one for iOS, are provided to illustrate the usage of SFBAudioEngine.

### macOS

[SimplePlayer for macOS](https://github.com/sbooth/SimplePlayer-macOS) is written in Swift using AppKit and supports gapless sequential playback of items from a playlist. The essential functionality is contained in one file, [PlayerWindowController.swift](https://github.com/sbooth/SimplePlayer-macOS/blob/main/SimplePlayer/PlayerWindowController.swift).

### iOS

[SimplePlayer for iOS](https://github.com/sbooth/SimplePlayer-iOS) is written in Swift using SwiftUI and supports playback of a single item selected from a list.

## License

SFBAudioEngine is released under the [MIT License](https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt).

The open-source projects providing support for the various audio formats are subject to their own licenses that are compatible with the MIT license when used with SFBAudioEngine's default build configuration.

### LGPL Notes

In order to maintain compatibility with the LGPL used by [libsndfile](http://libsndfile.github.io/libsndfile/), [mpg123](https://www.mpg123.de), [libtta-cpp](https://sourceforge.net/projects/tta/), [lame](https://lame.sourceforge.io), and the Musepack encoder dynamic linking is required.
