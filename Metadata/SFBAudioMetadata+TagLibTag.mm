/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata+TagLibTag.h"
#import "SFBAudioMetadata+Internal.h"

@implementation SFBAudioMetadata (TagLibTag)

- (void)addMetadataFromTagLibTag:(const TagLib::Tag *)tag
{
	NSParameterAssert(tag != nil);

	self.title = [NSString stringWithUTF8String:tag->title().toCString(true)];
	self.albumTitle = [NSString stringWithUTF8String:tag->album().toCString(true)];
	self.artist = [NSString stringWithUTF8String:tag->artist().toCString(true)];
	self.genre = [NSString stringWithUTF8String:tag->genre().toCString(true)];

	if(tag->year())
		self.releaseDate = @(tag->year()).stringValue;

	if(tag->track())
		self.trackNumber = @(tag->track());

	self.comment = [NSString stringWithUTF8String:tag->comment().toCString(true)];
}

@end
