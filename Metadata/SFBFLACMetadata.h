/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import "SFBAudioMetadata+Internal.h"

// An SFBAudioMetadataInputOutputHandler supporting FLAC files
@interface SFBFLACMetadata : NSObject <SFBAudioMetadataInputOutputHandling>
@end
