//
// Copyright (c) 2020 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import Foundation;

/// This class only exists so SimplePlayer won't be a pure Swift executable, making ubsan available for debugging SFBAudioEngine
@interface SFBDummyClass : NSObject
@end
