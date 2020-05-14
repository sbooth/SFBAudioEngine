#!/bin/sh

SQLITE_ARCHIVE=sqlite-src-3310100.zip
SQLITE_DOWNLOAD_URL=https://sqlite.org/2020/$SQLITE_ARCHIVE
SQLITE_DIR=$(basename "$SQLITE_ARCHIVE" .zip)

if ! [ -x "$(command -v jazzy)" ]; then
	echo "Error: jazzy not present"
	exit 1
fi

xcodebuild -project ./SFBAudioEngine.xcodeproj -configuration Debug

/bin/ln -s ./build/Debug/SFBAudioEngine.framework/Headers ./SFBAudioEngine

jazzy

/bin/rm ./SFBAudioEngine
