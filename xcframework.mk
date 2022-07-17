##
## Copyright (c) 2021 - 2022 Stephen F. Booth <stephen@sbooth.name>
## MIT license
##

## Generate an XCFramework from an Xcode project file using xcodebuild.
##
## This is a portion of a Makefile. To use it, include it in
## your own Makefile and set the following variables:
##
##   FRAMEWORK_NAME    The base name of the XCFramework to build.
##   XCODEPROJ         The path to the Xcode project file.
##
## The following targets are provided:
##
##   xcframework       Builds the XCFramework.
##   clean             Deletes the XCFramework and build products.
##   xz                Builds the XCFramework and compresses it.
##
## The following variables are optional:
##
##   MACOS_SCHEME      The scheme building the macOS framework target.
##                     If not set the default is "macOS".
##   IOS_SCHEME        The scheme building the isOS framework target.
##                     If not set the default is "iOS".
##   BUILD_DIR         The directory where the build products should
##                     be written. If not set the default is "build".
##
## The following targets are provided when MAKELEVEL is not 0:
##
##   install           Copies the XCFramework to PREFIX.
##   uninstall         Deletes the XCFramework from PREFIX.
##
## The following variable should be set:
##
##   PREFIX            The path of the desired installation prefix.
##                     There is no default value.
##
## An example Makefile could look like:
##
##   FRAMEWORK_NAME := SFBAudioUtilities
##   XCODEPROJ := SFBAudioUtilities.xcodeproj
##
##   MACOS_SCHEME := macOS Framework
##   IOS_SCHEME := iOS Framework
##
##   include ./xcframework.mk
##
## Running 'make' will create './SFBAudioUtilities.xcframework'.

# The default name of the scheme that builds the framework for macOS
MACOS_SCHEME ?= macOS
# The default name of the scheme that builds the framework for iOS
IOS_SCHEME ?= iOS

# Build products directory
BUILD_DIR ?= build

# The directory for intermediate .xcarchives
XCARCHIVE_DIR := $(BUILD_DIR)

# The name of the output XCFramework
XCFRAMEWORK := $(BUILD_DIR)/$(FRAMEWORK_NAME).xcframework

XZ_FILE := $(XCFRAMEWORK).tar.xz

MACOS_XCARCHIVE := $(XCARCHIVE_DIR)/macOS.xcarchive
MACOS_CATALYST_XCARCHIVE := $(XCARCHIVE_DIR)/macOS-Catalyst.xcarchive
IOS_XCARCHIVE := $(XCARCHIVE_DIR)/iOS.xcarchive
IOS_SIMULATOR_XCARCHIVE := $(XCARCHIVE_DIR)/iOS-Simulator.xcarchive

XCARCHIVES := $(MACOS_XCARCHIVE) $(MACOS_CATALYST_XCARCHIVE) $(IOS_XCARCHIVE) $(IOS_SIMULATOR_XCARCHIVE)

xcframework: $(XCFRAMEWORK)
.PHONY: xcframework

clean:
	rm -Rf "$(MACOS_XCARCHIVE)" "$(MACOS_CATALYST_XCARCHIVE)" "$(IOS_XCARCHIVE)" "$(IOS_SIMULATOR_XCARCHIVE)" "$(XCFRAMEWORK)" "$(XZ_FILE)"
.PHONY: clean

xz: $(XZ_FILE)
.PHONY: xz

ifneq (0,$(MAKELEVEL))
install: xcframework uninstall
	cp -R "$(XCFRAMEWORK)" "$(PREFIX)"
.PHONY: install

uninstall:
	rm -Rf "$(PREFIX)/$(XCFRAMEWORK)"
.PHONY: uninstall
endif

$(MACOS_XCARCHIVE): $(XCODEPROJ)
	xcodebuild archive -project "$(XCODEPROJ)" -scheme "$(MACOS_SCHEME)" -destination generic/platform=macOS -archivePath "$(basename $@)"

$(MACOS_CATALYST_XCARCHIVE): $(XCODEPROJ)
	xcodebuild archive -project "$(XCODEPROJ)" -scheme "$(IOS_SCHEME)" -destination "platform=macOS,variant=Mac Catalyst" -archivePath "$(basename $@)"

$(IOS_XCARCHIVE): $(XCODEPROJ)
	xcodebuild archive -project "$(XCODEPROJ)" -scheme "$(IOS_SCHEME)" -destination generic/platform=iOS -archivePath "$(basename $@)"

$(IOS_SIMULATOR_XCARCHIVE): $(XCODEPROJ)
	xcodebuild archive -project "$(XCODEPROJ)" -scheme "$(IOS_SCHEME)" -destination "generic/platform=iOS Simulator" -archivePath "$(basename $@)"

$(XCFRAMEWORK): $(XCARCHIVES)
	rm -Rf "$@"
	xcodebuild -create-xcframework $(foreach xcarchive,$^,-framework "$(xcarchive)/Products/Library/Frameworks/$(FRAMEWORK_NAME).framework" -debug-symbols "$(realpath $(xcarchive)/dSYMs/$(FRAMEWORK_NAME).framework.dSYM)" ) -output "$@"

$(XZ_FILE): $(XCFRAMEWORK)
	cd $(BUILD_DIR) && tar cJf "$(notdir $@)" "$(notdir $<)"
