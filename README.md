About SFBAudioEngine
====================

SFBAudioEngine is a set of C++ classes enabling Mac OS X and iOS applications to easily play audio.  SFBAudioEngine supports the following formats:

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
* Musepack (Mac OS X only)
* Monkey's Audio (Mac OS X only)
* True Audio (Mac OS X only)
* All other formats supported natively by Core Audio

In addition to playback, SFBAudioEngine supports reading and writing of metadata for most supported formats.

SFBAudioEngine uses C++11 language features such as delegated constructors and range-based for, and C++11 STL features such as `std::unique_ptr`.  For this reason clang must be used to compile SFBAudioEngine and its dependencies, and clang's libc++ must be used as the C++ standard library.  Any application using SFBAudioEngine must also be compiled with clang and libc++.

Building SFBAudioEngine
=======================

1. Get the source code: `git clone https://github.com/sbooth/SFBAudioEngine.git`
2. Download the dependencies and unpack in the project's root: http://files.sbooth.org/SFBAudioEngine-dependencies.tar.bz2
3. Open the project and build!
