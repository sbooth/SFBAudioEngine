# SFBAudioEngine

SFBAudioEngine is a set of Objective-C and Objective-C++ classes enabling macOS (10.15+) and iOS (13.0+) applications to easily play audio. SFBAudioEngine supports the following formats:

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
* All formats supported by libsndfile
* All formats supported by Core Audio
* DSD to PCM conversion for DSD64

In addition to playback, SFBAudioEngine supports reading and writing of metadata for most supported formats.

## Building SFBAudioEngine

1. Clone the [SFBAudioEngine](https://github.com/sbooth/SFBAudioEngine) repository.
2. Download the [dependencies](https://files.sbooth.org/SFBAudioEngine-dependencies.tar.bz2) and decompress in the project's root
3. Open the project, build, and play something using SimplePlayer!

## Using SFBAudioEngine

Playing an audio file is as simple as:

~~~swift
import SFBAudioEngine
let player = AudioPlayer()
try? player.play(URL(fileURLWithPath: "example.flac"))
~~~

## Sample Audio Players

Two versions of SimplePlayer, one for macOS and one for iOS, are provided illustrate the usage of SFBAudioEngine.

### macOS

![Image of an audio player window](SimplePlayer/screenshot.png)

[SimplePlayer](SimplePlayer/) for macOS is written in Swift using AppKit and supports gapless sequential playback of items from a playlist. The essential functionality is contained in one file, [PlayerWindowController.swift](SimplePlayer/PlayerWindowController.swift).

### iOS

![Image of audio file playback progress](SimplePlayer-iOS/screenshot.png)

[SimplePlayer](SimplePlayer-iOS/) for iOS is written in Swift using SwiftUI and supports playback of a single item selected from a list.

## License

SFBAudioEngine is released under the [MIT License](https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt).
