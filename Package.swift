// swift-tools-version: 5.6
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
	name: "SFBAudioEngine",
	platforms: [
		.macOS(.v11),
		.iOS(.v15),
		.tvOS(.v15),
	],
	products: [
		.library(
			name: "SFBAudioEngine",
			targets: [
				"CSFBAudioEngine",
				"SFBAudioEngine",
			]),
	],
	dependencies: [
		.package(url: "https://github.com/sbooth/AVFAudioExtensions", from: "0.4.1"),
		.package(url: "https://github.com/sbooth/CXXCoreAudio", branch: "main"),
		.package(url: "https://github.com/sbooth/CXXOSUnfairLock", branch: "main"),
		.package(url: "https://github.com/sbooth/CXXRingBuffer", from: "0.1.0"),

		// Standalone dependencies from source
		.package(url: "https://github.com/sbooth/CDUMB", from: "2.0.3"),
		.package(url: "https://github.com/sbooth/CXXMonkeysAudio", from: "11.22.0"),
		.package(url: "https://github.com/sbooth/CXXTagLib", from: "2.1.0"),

		// Standalone dependencies not easily packaged using SPM
		.package(url: "https://github.com/sbooth/wavpack-binary-xcframework", from: "0.1.2"),

		// Xiph ecosystem
		.package(url: "https://github.com/sbooth/ogg-binary-xcframework", from: "0.1.2"),
		// flac-binary-xcframework requires ogg-binary-xcframework
		.package(url: "https://github.com/sbooth/flac-binary-xcframework", from: "0.2.0"),
		// opus-binary-xcframework requires ogg-binary-xcframework
		.package(url: "https://github.com/sbooth/opus-binary-xcframework", from: "0.2.2"),
		// vorbis-binary-xcframework requires ogg-binary-xcframework
		.package(url: "https://github.com/sbooth/vorbis-binary-xcframework", from: "0.1.2"),
		// libspeex does not depend on libogg
		.package(url: "https://github.com/sbooth/CSpeex", from: "1.2.1"),

		// LGPL bits
		.package(url: "https://github.com/sbooth/lame-binary-xcframework", from: "0.1.2"),
		// Technically only the musepack *encoder* is LGPL'd but for now the decoder and encoder are packaged together
		.package(url: "https://github.com/sbooth/mpc-binary-xcframework", from: "0.1.2"),
		.package(url: "https://github.com/sbooth/mpg123-binary-xcframework", from: "0.2.3"),
		// sndfile-binary-xcframework requires ogg-binary-xcframework, flac-binary-xcframework, opus-binary-xcframework, and vorbis-binary-xcframework
		.package(url: "https://github.com/sbooth/sndfile-binary-xcframework", from: "0.1.2"),
		.package(url: "https://github.com/sbooth/tta-cpp-binary-xcframework", from: "0.1.2"),
	],
	targets: [
		.target(
			name: "CSFBAudioEngine",
			dependencies: [
				.product(name: "AVFAudioExtensions", package: "AVFAudioExtensions"),
				.product(name: "CXXCoreAudio", package: "CXXCoreAudio"),
				.product(name: "CXXOSUnfairLock", package: "CXXOSUnfairLock"),
				.product(name: "CXXRingBuffer", package: "CXXRingBuffer"),
				// Standalone dependencies
				.product(name: "dumb", package: "CDUMB"),
				.product(name: "MAC", package: "CXXMonkeysAudio"),
				.product(name: "taglib", package: "CXXTagLib"),
				.product(name: "wavpack", package: "wavpack-binary-xcframework"),
				// Xiph ecosystem
				.product(name: "ogg", package: "ogg-binary-xcframework"),
				.product(name: "FLAC", package: "flac-binary-xcframework"),
				.product(name: "opus", package: "opus-binary-xcframework"),
				.product(name: "vorbis", package: "vorbis-binary-xcframework"),
				.product(name: "speex", package: "CSpeex"),
				// LGPL bits
				.product(name: "lame", package: "lame-binary-xcframework"),
				.product(name: "mpc", package: "mpc-binary-xcframework"),
				.product(name: "mpg123", package: "mpg123-binary-xcframework"),
				.product(name: "sndfile", package: "sndfile-binary-xcframework"),
				.product(name: "tta-cpp", package: "tta-cpp-binary-xcframework"),
			],
			cSettings: [
				.headerSearchPath("include/SFBAudioEngine"),
				.headerSearchPath("Input"),
				.headerSearchPath("Decoders"),
				.headerSearchPath("Player"),
				.headerSearchPath("Output"),
				.headerSearchPath("Encoders"),
				.headerSearchPath("Utilities"),
				.headerSearchPath("Analysis"),
				.headerSearchPath("Metadata"),
				.headerSearchPath("Conversion"),
			],
			linkerSettings: [
				.linkedFramework("Accelerate"),
				.linkedFramework("AudioToolbox"),
				.linkedFramework("AVFAudio"),
//				.linkedFramework("CoreAudioTypes"),
				.linkedFramework("Foundation"),
				.linkedFramework("ImageIO"),
				.linkedFramework("UniformTypeIdentifiers"),
			]),
		.target(
			name: "SFBAudioEngine",
			dependencies: [
				"CSFBAudioEngine",
			]),
		.testTarget(
			name: "SFBAudioEngineTests",
			dependencies: [
				"SFBAudioEngine",
			])
	],
	cLanguageStandard: .c11,
	cxxLanguageStandard: .cxx20
)
