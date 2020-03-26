/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBAudioMetadata+TagLibTag.h"

@implementation SFBAudioMetadata (TagLibID3v1Tag)

- (void)addMetadataFromTagLibID3v1Tag:(const TagLib::ID3v1::Tag *)tag
{
	NSParameterAssert(tag != nil);

	// ID3v1 tags are only supposed to contain characters in ISO 8859-1 format, but that isn't always the case
	// AddTagToDictionary assumes UTF-8, so everything should work properly
	// Currently TagLib::ID3v1::Tag doesn't implement any more functionality than TagLib::Tag
	[self addMetadataFromTagLibTag:tag];
}

@end

void SFB::Audio::SetID3v1TagFromMetadata(SFBAudioMetadata *metadata, TagLib::ID3v1::Tag *tag)
{
	// TagLib::ID3v1::Tag has no additonal functionality over TagLib::Tag
	SetTagFromMetadata(metadata, tag);
}
