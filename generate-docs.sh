#!/bin/sh

JAZZY_FRAMEWORK_ROOT=SFBAudioEngine

if ! [ -x "$(command -v jazzy)" ]; then
	echo "Error: jazzy not present"
	exit 1
fi

xcodebuild -project ./SFBAudioEngine.xcodeproj -configuration Debug

# jazzy doesn't like headers in multiple directories
/bin/ln -s ./build/Debug/SFBAudioEngine.framework/Headers ./$JAZZY_FRAMEWORK_ROOT

jazzy

/bin/rm ./$JAZZY_FRAMEWORK_ROOT
