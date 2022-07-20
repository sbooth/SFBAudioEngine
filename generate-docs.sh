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
sourcekitten doc -- -project SFBAudioEngine.xcodeproj -scheme "macOS Framework" > swiftDoc.json

# Generate Objective-C SourceKitten output
# jazzy doesn't like headers in multiple directories
xcodebuild -project ./SFBAudioEngine.xcodeproj -scheme "macOS Framework"
sourcekitten doc --objc ./SFBAudioEngine.h \
		-- -x objective-c -isysroot $(xcrun --show-sdk-path --sdk macosx) \
		-I ./build/Debug -F ./build/Debug > objcDoc.json

# Feed both outputs to Jazzy as a comma-separated list
jazzy --sourcekitten-sourcefile swiftDoc.json,objcDoc.json

/bin/rm -f swiftDoc.json objcDoc.json
