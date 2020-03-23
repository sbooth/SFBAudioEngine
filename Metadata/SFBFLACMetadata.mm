/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <memory>

#import <taglib/flacfile.h>
#import <taglib/tfilestream.h>

#import "CFWrapper.h"
#import "SetID3v1TagFromMetadata.h"
#import "SetID3v2TagFromMetadata.h"
#import "SetXiphCommentFromMetadata.h"
#import "TagLibStringUtilities.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+Internal.h"
#import "SFBAudioMetadata+TagLibAudioProperties.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBAudioMetadata+TagLibXiphComment.h"
#import "SFBFLACMetadata.h"

@implementation SFBFLACMetadata

+ (void)load
{
	[SFBAudioMetadata registerSubclass:[self class]];
}

+ (NSArray *)_supportedFileExtensions
{
	return @[@"flac"];
}

+ (NSArray *)_supportedMIMETypes
{
	return @[@"audio/flac"];
}

- (BOOL)_readMetadata:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(self.url.fileSystemRepresentation, true));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for reading.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::FLAC::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid FLAC file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not a FLAC file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	self.formatName = @"FLAC";

	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		[self addAudioPropertiesFromTagLibAudioProperties:properties];

		if(properties->bitsPerSample())
			self.bitsPerChannel = @(properties->bitsPerSample());
		if(properties->sampleFrames())
			self.totalFrames = @(properties->sampleFrames());
	}

	// Add all tags that are present
	if(file.hasID3v1Tag())
		[self addMetadataFromTagLibID3v1Tag:file.ID3v1Tag()];

	if(file.hasID3v2Tag())
		[self addMetadataFromTagLibID3v2Tag:file.ID3v2Tag()];

	if(file.hasXiphComment())
		[self addMetadataFromTagLibXiphComment:file.xiphComment()];

	// Add album art
	for(auto iter : file.pictureList()) {
		NSData *imageData = [NSData dataWithBytes:iter->data().data() length:iter->data().size()];

		NSString *description = nil;
		if(!iter->description().isEmpty())
			description = [NSString stringWithUTF8String:iter->description().toCString(true)];

		[self attachPicture:[[SFBAttachedPicture alloc] initWithImageData:imageData
																	 type:(SFBAttachedPictureType)iter->type()
															  description:description]];
	}

	return YES;
}

- (BOOL)_writeMetadata:(NSError **)error
{
	std::unique_ptr<TagLib::FileStream> stream(new TagLib::FileStream(self.url.fileSystemRepresentation));
	if(!stream->isOpen()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for writing.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::FLAC::File file(stream.get());
	if(!file.isValid()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid FLAC file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not a FLAC file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// ID3v1 and ID3v2 tags are only written if present, but a Xiph comment is always written

	if(file.hasID3v1Tag())
		SFB::Audio::SetID3v1TagFromMetadata(self, file.ID3v1Tag());

	if(file.hasID3v2Tag())
		SFB::Audio::SetID3v2TagFromMetadata(self, file.ID3v2Tag());

	SFB::Audio::SetXiphCommentFromMetadata(self, file.xiphComment());

	if(!file.save()) {
		if(error)
			*error = [NSError sfb_errorWithDomain:SFBAudioMetadataErrorDomain
											 code:SFBAudioMetadataErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Unable to write metadata", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// Add album art
	for(SFBAttachedPicture *attachedPicture in self.attachedPictures) {
		SFB::CGImageSource imageSource(CGImageSourceCreateWithData((__bridge CFDataRef)attachedPicture.imageData, nullptr));
		if(!imageSource)
			continue;

		TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
		picture->setData(TagLib::ByteVector((const char *)attachedPicture.imageData.bytes, (size_t)attachedPicture.imageData.length));
		picture->setType((TagLib::FLAC::Picture::Type)attachedPicture.pictureType);
		if(attachedPicture.pictureDescription)
			picture->setDescription(TagLib::StringFromNSString(attachedPicture.pictureDescription));

		// Convert the image's UTI into a MIME type
		NSString *mimeType = (__bridge_transfer NSString *)UTTypeCopyPreferredTagWithClass(CGImageSourceGetType(imageSource), kUTTagClassMIMEType);
		if(mimeType)
			picture->setMimeType(TagLib::StringFromNSString(mimeType));

		// Flesh out the height, width, and depth
		NSDictionary *imagePropertiesDictionary = (__bridge_transfer NSDictionary *)CGImageSourceCopyPropertiesAtIndex(imageSource, 0, nullptr);
		if(imagePropertiesDictionary) {
			NSNumber *imageWidth = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyPixelWidth];
			NSNumber *imageHeight = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyPixelHeight];
			NSNumber *imageDepth = imagePropertiesDictionary[(__bridge NSString *)kCGImagePropertyDepth];

			picture->setHeight(imageHeight.intValue);
			picture->setWidth(imageWidth.intValue);
			picture->setColorDepth(imageDepth.intValue);
		}

		file.addPicture(picture);
	}

	return YES;
}

@end
