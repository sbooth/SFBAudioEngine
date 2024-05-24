// swift-tools-version: 5.6
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
	name: "SFBAudioEngine",
	platforms: [
		.macOS(.v10_15),
		.iOS(.v14),
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
		.package(url: "https://github.com/sbooth/CXXAudioUtilities", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/AVFAudioExtensions", from: "0.1.0"),
		// Standalone dependencies from source
		.package(url: "https://github.com/sbooth/CDUMB", from: "2.0.3"),
		.package(url: "https://github.com/sbooth/CWavPack", from: "5.7.0"),
		.package(url: "https://github.com/sbooth/CXXMonkeysAudio", from: "10.71.0"),
		.package(url: "https://github.com/sbooth/CXXTagLib", from: "2.0.1"),
		// Xiph ecosystem XCFrameworks
		.package(url: "https://github.com/sbooth/flac-binary-xcframework", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/ogg-binary-xcframework", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/opus-binary-xcframework", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/speex-binary-xcframework", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/vorbis-binary-xcframework", from: "0.1.0"),
		// LGPL bits
		.package(url: "https://github.com/sbooth/lame-binary-xcframework", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/mpc-binary-xcframework", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/mpg123-binary-xcframework", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/sndfile-binary-xcframework", from: "0.1.0"),
		.package(url: "https://github.com/sbooth/tta-cpp-binary-xcframework", from: "0.1.0"),
	],
	targets: [
		.target(
			name: "CSFBAudioEngine",
			dependencies: [
				.product(name: "CXXAudioUtilities", package: "CXXAudioUtilities"),
				.product(name: "AVFAudioExtensions", package: "AVFAudioExtensions"),
				// Standalone dependencies
				.product(name: "dumb", package: "CDUMB"),
				.product(name: "wavpack", package: "CWavPack"),
				.product(name: "MAC", package: "CXXMonkeysAudio"),
				.product(name: "taglib", package: "CXXTagLib"),
				// Xiph ecosystem
				.product(name: "FLAC", package: "flac-binary-xcframework"),
				.product(name: "ogg", package: "ogg-binary-xcframework"),
				.product(name: "opus", package: "opus-binary-xcframework"),
				.product(name: "speex", package: "speex-binary-xcframework"),
				.product(name: "vorbis", package: "vorbis-binary-xcframework"),
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
				.linkedFramework("Foundation"),
				.linkedFramework("AVFAudio"),
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
	cxxLanguageStandard: .cxx17
)
