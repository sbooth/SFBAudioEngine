# SFBAudioEngine

SFBAudioEngine is a framework for macOS and iOS audio playback using Swift or Objective-C. SFBAudioEngine supports the following formats:

* WAVE
* AIFF
* Apple Lossless
* AAC
* FLAC
* MP3
* WavPack
* Ogg Vorbis
* Ogg Speex
* Ogg Opus
* Musepack
* Monkey's Audio
* True Audio
* Shorten
* All formats supported by libsndfile
* All formats supported by Core Audio
* DSD to PCM conversion for DSD64

In addition to playback SFBAudioEngine supports reading and writing of metadata for most supported formats.

## Requirements

macOS 10.15+ or iOS 14.0+

## Building SFBAudioEngine

1. `git clone git@github.com:sbooth/SFBAudioEngine.git --recurse-submodules`
2. `cd SFBAudioEngine`
3. `make -C XCFrameworks install`

The project file contains targets for macOS and iOS frameworks.

The included `Makefile` may also be used to create the build products:

### macOS Framework Build

`make archive/macOS.xcarchive`

### macOS Catalyst Framework Build

`make archive/macOS-Catalyst.xcarchive`

### iOS Framework Build

`make archive/iOS.xcarchive`

### iOS Simulator Framework Build

`make archive/iOS-Simulator.xcarchive`

### XCFramework Build

`make`

### SimplePlayer

Open [SimplePlayer](SimplePlayer-macOS/), build, and play something!

## Quick Start

Playing an audio file is as simple as:

~~~swift
import SFBAudioEngine
let player = AudioPlayer()
let url = URL(fileURLWithPath: "example.flac")
try? player.play(url)
~~~

Reading audio properties and metadata is similarly trivial:

~~~swift
if let audioFile = try? AudioFile(readingPropertiesAndMetadataFrom: url) {
    let sampleRate = audioFile.properties.sampleRate
    let title = audioFile.metadata.title
}
~~~

## Design

### [Audio Decoders](Decoders/)

Audio decoders in SFBAudioEngine are broadly divided into two categories, those producing PCM output and those producing DSD output. Audio decoders read data from an [SFBInputSource](Input/SFBInputSource.h) which may refer to a file, buffer, or network source.

All audio decoders in SFBAudioEngine implement the [SFBAudioDecoding](Decoders/SFBAudioDecoding.h) protocol. PCM decoders additionally implement [SFBPCMDecoding](Decoders/SFBPCMDecoding.h) while DSD decoders implement [SFBDSDDecoding](Decoders/SFBDSDDecoding.h).

Three special decoder subclasses that wrap an underlying audio decoder instance are also provided: [SFBLoopableRegionDecoder](Decoders/SFBLoopableRegionDecoder.h), [SFBDoPDecoder](Decoders/SFBDoPDecoder.h), and [SFBDSDPCMDecoder](Decoders/SFBDSDPCMDecoder.h). For seekable inputs, [SFBLoopableRegionDecoder](Decoders/SFBLoopableRegionDecoder.h) allows arbitrary looping and repeating of a specified PCM decoder segment. [SFBDoPDecoder](Decoders/SFBDoPDecoder.h) and [SFBDSDPCMDecoder](Decoders/SFBDSDPCMDecoder.h) wrap a DSD decoder providing DSD over PCM (DoP) and PCM output respectively.

### [SFBAudioPlayerNode](Player/SFBAudioPlayerNode.h)

[SFBAudioPlayerNode](Player/SFBAudioPlayerNode.h) is a subclass of [AVAudioSourceNode](https://developer.apple.com/documentation/avfoundation/avaudiosourcenode) that provides rich playback functionality within an [AVAudioEngine](https://developer.apple.com/documentation/avfoundation/avaudioengine) processing graph. [SFBAudioPlayerNode](Player/SFBAudioPlayerNode.h) supports gapless playback and comprehensive status notifications through delegate callbacks.

### [SFBAudioPlayer](Player/SFBAudioPlayer.h)

[SFBAudioPlayer](Player/SFBAudioPlayer.h) wraps an [AVAudioEngine](https://developer.apple.com/documentation/avfoundation/avaudioengine) processing graph driven by [SFBAudioPlayerNode](Player/SFBAudioPlayerNode.h). [SFBAudioPlayer](Player/SFBAudioPlayer.h) provides complete player functionality with no required configuration but also allows customization of the underlying processing graph as well as rich status notifications through delegate callbacks.

### [Audio Properties and Metadata](Metadata/)

Audio properties and metadata are accessed from instances of [SFBAudioFile](Metadata/SFBAudioFile.h). [Audio properties](Metadata/SFBAudioProperties.h) are read-only while [metadata](Metadata/AudioMetada.h) is writable for most formats.

## Sample Audio Players

Two versions of SimplePlayer, one for macOS and one for iOS, are provided illustrate the usage of SFBAudioEngine.

### macOS

![Image of an audio player window](SimplePlayer-macOS/screenshot.png)

[SimplePlayer](SimplePlayer-macOS/) for macOS is written in Swift using AppKit and supports gapless sequential playback of items from a playlist. The essential functionality is contained in one file, [PlayerWindowController.swift](SimplePlayer-macOS/PlayerWindowController.swift).

### iOS

![Image of audio file playback progress](SimplePlayer-iOS/screenshot.png)

[SimplePlayer](SimplePlayer-iOS/) for iOS is written in Swift using SwiftUI and supports playback of a single item selected from a list.

## License

SFBAudioEngine is released under the [MIT License](https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt).
