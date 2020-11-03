/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import Foundation;

/// This class only exists so SimplePlayer won't be a pure Swift executable, making ubsan available for debugging SFBAudioEngine
@interface SFBDummyClass : NSObject
@end
