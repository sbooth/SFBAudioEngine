//
// Copyright (c) 2023-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <CoreFoundation/CoreFoundation.h>

/// A three-valued logic truth value (AKA tribool)
/// No assumption is made whether unknown implies true
typedef CF_CLOSED_ENUM(NSInteger, SFBTernaryTruthValue) {
	/// True
	SFBTernaryTruthValueTrue 		CF_SWIFT_NAME(ternaryTrue) = 1,
	/// False
	SFBTernaryTruthValueFalse 		CF_SWIFT_NAME(ternaryFalse) = 0,
	/// Unknown
	SFBTernaryTruthValueUnknown 	CF_SWIFT_NAME(ternaryUnknown) = -1,
} CF_SWIFT_NAME(TernaryTruthValue);
