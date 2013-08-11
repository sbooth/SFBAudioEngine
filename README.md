SFBAudioEngine is a set of C++ classes enabling Mac OS X and iOS applications to easily play audio in the following formats:

* WAVE
* AIFF
* Apple Lossless
* AAC
* FLAC
* MP3
* Musepack
* WavePack
* Ogg Vorbis
* Monkey's Audio
* Ogg Speex
* True Audio
* All other formats supported natively by Core Audio

In addition to playback, SFBAudioEngine supports reading and writing of metadata for most supported formats.

SFBAudioEngine uses C++11 language features such as delegated constructors and range-based for, and C++11 STL features such as `std::unique_ptr`.  For this reason clang must be used to compile SFBAudioEngine and its dependencies, and clang's libc++ must be used as the C++ standard library.  Any application using SFBAudioEngine must also be compiled with clang and libc++.
