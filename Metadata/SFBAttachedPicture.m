/*
 * Copyright (c) 2012 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAttachedPicture.h"

// Key names for the metadata dictionary
SFBAttachedPictureKey const SFBAttachedPictureKeyImageData		= @"Image Data";
SFBAttachedPictureKey const SFBAttachedPictureKeyType			= @"Type";
SFBAttachedPictureKey const SFBAttachedPictureKeyDescription	= @"Description";

@implementation SFBAttachedPicture

- (instancetype)initWithImageData:(NSData *)imageData
{
	return [self initWithImageData:imageData type:SFBAttachedPictureTypeOther description:nil];
}

- (instancetype)initWithImageData:(NSData *)imageData type:(SFBAttachedPictureType)type
{
	return [self initWithImageData:imageData type:type description:nil];
}

- (instancetype)initWithImageData:(NSData *)imageData type:(SFBAttachedPictureType)type description:(NSString *)description
{
	NSParameterAssert(imageData != nil);

	if((self = [super init])) {
		_imageData = [imageData copy];
		_pictureType = type;
		_pictureDescription = [description copy];
	}
	return self;
}

- (instancetype)initWithDictionaryRepresentation:(NSDictionary *)dictionaryRepresentation
{
	if(!dictionaryRepresentation[SFBAttachedPictureKeyImageData])
		return nil;

	return [self initWithImageData:dictionaryRepresentation[SFBAttachedPictureKeyImageData]
							  type:[dictionaryRepresentation[SFBAttachedPictureKeyType] unsignedIntegerValue]
					   description:dictionaryRepresentation[SFBAttachedPictureKeyDescription]];
}

- (BOOL)isEqual:(id)object
{
	if(![object isKindOfClass:[SFBAttachedPicture class]])
		return NO;

	SFBAttachedPicture *other = (SFBAttachedPicture *)object;
	return (self.imageData == other.imageData || [self.imageData isEqual:other.imageData]) && self.pictureType == other.pictureType;
}

- (NSUInteger)hash
{
	return self.imageData.hash ^ self.pictureType;
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone
{
#pragma unused(zone)
	return self;
}

- (NSDictionary *)dictionaryRepresentation
{
	return @{ SFBAttachedPictureKeyImageData: self.imageData,
			  SFBAttachedPictureKeyType: @(self.pictureType),
			  SFBAttachedPictureKeyDescription: self.pictureDescription };
}

@end
