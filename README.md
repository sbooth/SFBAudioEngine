About SFBAudioEngine
====================

SFBAudioEngine is a set of C++ classes enabling macOS (10.13+) and iOS applications to easily play audio.  SFBAudioEngine supports the following formats:

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
* Musepack (macOS only)
* Monkey's Audio (macOS only)
* True Audio (macOS only)
* All formats supported by libsndfile
* All formats supported by Core Audio
* DSD to PCM conversion for DSD64
* DSD over PCM (DoP) for DSF and DSDIFF on supported hardware

In addition to playback, SFBAudioEngine supports reading and writing of metadata for most supported formats.

Building SFBAudioEngine
=======================

1. Get the source code: `git clone https://github.com/sbooth/SFBAudioEngine.git`
2. Download the dependencies and unpack in the project's root: http://files.sbooth.org/SFBAudioEngine-dependencies.tar.bz2
3. Open the project and build!

Using SFBAudioEngine
====================

Playing an audio file is as simple as:

~~~
NSURL *u = [NSURL fileURLWithPath:@"example.flac" isDirectory:NO];
SFB::Audio::Player player;
player.PlayURL((__bridge CFURLRef)u);
~~~

Documentation
=============

All public headers are documented using doxygen.  If you have doxygen installed, you may create a local copy of the documentation by running `doxygen` in SFBAudioEngine's root.  The HTML files will be saved in `doc/html/`.

The [documentation is also available online](http://sbooth.github.io/SFBAudioEngine/doc/).
