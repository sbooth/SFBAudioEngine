#!/bin/sh

if ! [ -x "$(command -v sourcekitten)" ]; then
	echo "Error: sourcekitten not present"
	exit 1
fi

if ! [ -x "$(command -v jazzy)" ]; then
	echo "Error: jazzy not present"
	exit 1
fi

# Generate Swift SourceKitten output
sourcekitten doc -- -project SFBAudioEngine.xcodeproj -target "macOS Framework" -arch x86_64 -configuration Debug > swiftDoc.json

# Generate Objective-C SourceKitten output
# jazzy doesn't like headers in multiple directories
xcodebuild -project ./SFBAudioEngine.xcodeproj -target "macOS Framework" -arch x86_64 -configuration Debug
sourcekitten doc --objc SFBAudioEngine.h \
		-- -x objective-c -isysroot $(xcrun --show-sdk-path --sdk macosx) \
		-I ./build/Debug -F ./build/Debug > objcDoc.json

# Feed both outputs to Jazzy as a comma-separated list
jazzy --sourcekitten-sourcefile swiftDoc.json,objcDoc.json

/bin/rm -f swiftDoc.json objcDoc.json
