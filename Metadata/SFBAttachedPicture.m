/*
 * Copyright (c) 2012 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAttachedPicture.h"

// Key names for the metadata dictionary
NSString * const SFBAttachedPictureImageDataKey				= @"Image Data";
NSString * const SFBAttachedPictureTypeKey					= @"Type";
NSString * const SFBAttachedPictureDescriptionKey			= @"Description";

@implementation SFBAttachedPicture

+ (instancetype)attachedPictureFromDictionaryRepresentation:(NSDictionary *)dictionary
{
	return [[SFBAttachedPicture alloc] initWithImageData:dictionary[SFBAttachedPictureImageDataKey]
													type:[dictionary[SFBAttachedPictureTypeKey] unsignedIntegerValue]
											 description:dictionary[SFBAttachedPictureDescriptionKey]];
}

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
	if((self = [super init])) {
		_imageData = [imageData copy];
		_pictureType = type;
		_pictureDescription = [description copy];
	}
	return self;
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

- (NSDictionary *)dictionaryRepresentation
{
	return @{ SFBAttachedPictureImageDataKey: self.imageData,
			  SFBAttachedPictureTypeKey: @(self.pictureType),
			  SFBAttachedPictureDescriptionKey: self.pictureDescription };
}

@end
