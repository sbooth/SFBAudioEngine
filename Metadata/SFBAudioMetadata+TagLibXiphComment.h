/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <taglib/xiphcomment.h>

#import "SFBAudioMetadata.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioMetadata (TagLibXiphComment)
- (void)addMetadataFromTagLibXiphComment:(const TagLib::Ogg::XiphComment *)tag;
@end

NS_ASSUME_NONNULL_END
