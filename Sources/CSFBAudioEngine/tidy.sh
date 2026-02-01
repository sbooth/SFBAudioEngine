#!/bin/sh

TIDY="/usr/local/opt/llvm/bin/clang-tidy"
#CHECKS="-*,-readability-*,readability-implicit-bool-conversion"
CHECKS="-*,bugprone-*"

SRCDIR="/Users/sbooth/Development/SFBAudioEngine"
CSRCDIR="$SRCDIR/Sources/CSFBAudioEngine"

CHECKOUTS="$SRCDIR/.build/checkouts"
ARTIFACTS="$SRCDIR/.build/artifacts"

OBJC_SRCS="$CSRCDIR/**/*.m"
OBJCXX_SRCS="$CSRCDIR/**/*.mm"
OBJCXX_HEADERS="$CSRCDIR/Player/*.h"
CXX_HEADERS="$CSRCDIR/**/*.hpp"

## Objective-C

$TIDY \
--checks="$CHECKS" \
$OBJC_SRCS \
--format-style=file \
-- \
-I. \
-I$CSRCDIR/include \
-I$CSRCDIR/include/SFBAudioEngine \
-I$CSRCDIR/Analysis \
-I$CSRCDIR/Conversion \
-I$CSRCDIR/Decoders \
-I$CSRCDIR/Encoders \
-I$CSRCDIR/Input \
-I$CSRCDIR/Metadata \
-I$CSRCDIR/Output \
-I$CSRCDIR/Player \
-I$CSRCDIR/Utilities \
-I$CHECKOUTS/AVFAudioExtensions/Sources/AVFAudioExtensions/include \
-I$CHECKOUTS/CDUMB/Sources/dumb/include \
-I$CHECKOUTS/CSpeex/Sources/speex/include \
-F$ARTIFACTS/flac-binary-xcframework/flac/flac.xcframework/macos-arm64_x86_64 \
-framework flac \
-F$ARTIFACTS/lame-binary-xcframework/lame/lame.xcframework/macos-arm64_x86_64 \
-framework lame \
-F$ARTIFACTS/mpc-binary-xcframework/mpc/mpc.xcframework/macos-arm64_x86_64 \
-framework mpc \
-F$ARTIFACTS/mpg123-binary-xcframework/mpg123/mpg123.xcframework/macos-arm64_x86_64 \
-framework mpg123 \
-F$ARTIFACTS/ogg-binary-xcframework/ogg/ogg.xcframework/macos-arm64_x86_64 \
-framework ogg \
-F$ARTIFACTS/opus-binary-xcframework/opus/opus.xcframework/macos-arm64_x86_64 \
-framework opus \
-F$ARTIFACTS/sndfile-binary-xcframework/sndfile/sndfile.xcframework/macos-arm64_x86_64 \
-framework sndfile \
-F$ARTIFACTS/vorbis-binary-xcframework/vorbis/vorbis.xcframework/macos-arm64_x86_64 \
-framework vorbis \
-F$ARTIFACTS/wavpack-binary-xcframework/wavpack/wavpack.xcframework/macos-arm64_x86_64 \
-framework wavpack

## C++

# $TIDY \
# --checks="$CHECKS" \
# $OBJCXX_SRCS \
# --format-style=file \
# --fix \
# -- \
# -x objective-c++ \
# -fobjc-arc \
# -std=c++20 \
# -I. \
# -I$CSRCDIR/include \
# -I$CSRCDIR/include/SFBAudioEngine \
# -I$CSRCDIR/Analysis \
# -I$CSRCDIR/Conversion \
# -I$CSRCDIR/Decoders \
# -I$CSRCDIR/Encoders \
# -I$CSRCDIR/Input \
# -I$CSRCDIR/Metadata \
# -I$CSRCDIR/Output \
# -I$CSRCDIR/Player \
# -I$CSRCDIR/Utilities \
# -I$CHECKOUTS/AVFAudioExtensions/Sources/AVFAudioExtensions/include \
# -I$CHECKOUTS/CDUMB/Sources/dumb/include \
# -I$CHECKOUTS/CSpeex/Sources/speex/include \
# -I$CHECKOUTS/CXXAudioToolbox/Sources/CXXAudioToolbox/include \
# -I$CHECKOUTS/CXXCoreAudio/Sources/CXXCoreAudio/include \
# -I$CHECKOUTS/CXXMonkeysAudio/Sources/MAC/include \
# -I$CHECKOUTS/CXXRingBuffer/Sources/CXXRingBuffer/include \
# -I$CHECKOUTS/CXXTagLib/Sources/taglib/include \
# -I$CHECKOUTS/CXXUnfairLock/Sources/CXXUnfairLock/include \
# -F$ARTIFACTS/flac-binary-xcframework/FLAC/FLAC.xcframework/macos-arm64_x86_64 \
# -framework FLAC \
# -F$ARTIFACTS/lame-binary-xcframework/lame/lame.xcframework/macos-arm64_x86_64 \
# -framework lame \
# -F$ARTIFACTS/mpc-binary-xcframework/mpc/mpc.xcframework/macos-arm64_x86_64 \
# -framework mpc \
# -F$ARTIFACTS/mpg123-binary-xcframework/mpg123/mpg123.xcframework/macos-arm64_x86_64 \
# -framework mpg123 \
# -F$ARTIFACTS/ogg-binary-xcframework/ogg/ogg.xcframework/macos-arm64_x86_64 \
# -framework ogg \
# -F$ARTIFACTS/opus-binary-xcframework/opus/opus.xcframework/macos-arm64_x86_64 \
# -framework opus \
# -F$ARTIFACTS/sndfile-binary-xcframework/sndfile/sndfile.xcframework/macos-arm64_x86_64 \
# -framework sndfile \
# -F$ARTIFACTS/tta-cpp-binary-xcframework/tta-cpp/tta-cpp.xcframework/macos-arm64_x86_64 \
# -framework tta-cpp \
# -F$ARTIFACTS/vorbis-binary-xcframework/vorbis/vorbis.xcframework/macos-arm64_x86_64 \
# -framework vorbis \
# -F$ARTIFACTS/wavpack-binary-xcframework/wavpack/wavpack.xcframework/macos-arm64_x86_64 \
# -framework wavpack
