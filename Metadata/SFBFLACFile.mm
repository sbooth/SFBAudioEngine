//
// Copyright (c) 2006 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <memory>

#import <CoreServices/CoreServices.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"

#import <taglib/flacfile.h>
#import <taglib/tfilestream.h>

#pragma clang diagnostic pop

#import "SFBFLACFile.h"

#import "SFBCFWrapper.hpp"

#import "AddAudioPropertiesToDictionary.h"
#import "NSError+SFBURLPresentation.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"
#import "SFBAudioMetadata+TagLibID3v2Tag.h"
#import "SFBAudioMetadata+TagLibXiphComment.h"
#import "TagLibStringUtilities.h"

SFBAudioFileFormatName const SFBAudioFileFormatNameFLAC = @"org.sbooth.AudioEngine.File.FLAC";

@implementation SFBFLACFile

+ (void)load
{
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"flac"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/flac"];
}

+ (SFBAudioFileFormatName)formatName
{
	return SFBAudioFileFormatNameFLAC;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error
{
	TagLib::FileStream stream(self.url.fileSystemRepresentation, true);
	if(!stream.isOpen()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for reading.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::FLAC::File file(&stream);
	if(!file.isValid()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid FLAC file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not a FLAC file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"FLAC" forKey:SFBAudioPropertiesKeyFormatName];
	if(file.audioProperties()) {
		auto properties = file.audioProperties();
		SFB::Audio::AddAudioPropertiesToDictionary(properties, propertiesDictionary);

		if(properties->bitsPerSample())
			propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
		if(properties->sampleFrames())
			propertiesDictionary[SFBAudioPropertiesKeyFrameLength] = @(properties->sampleFrames());
	}

	// Add all tags that are present
	SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
	if(file.hasID3v1Tag())
		[metadata addMetadataFromTagLibID3v1Tag:file.ID3v1Tag()];

	if(file.hasID3v2Tag())
		[metadata addMetadataFromTagLibID3v2Tag:file.ID3v2Tag()];

	if(file.hasXiphComment())
		[metadata addMetadataFromTagLibXiphComment:file.xiphComment()];

	// Add album art
	for(auto iter : file.pictureList()) {
		NSData *imageData = [NSData dataWithBytes:iter->data().data() length:iter->data().size()];

		NSString *description = nil;
		if(!iter->description().isEmpty())
			description = [NSString stringWithUTF8String:iter->description().toCString(true)];

		[metadata attachPicture:[[SFBAttachedPicture alloc] initWithImageData:imageData
																	 type:(SFBAttachedPictureType)iter->type()
															  description:description]];
	}

	self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
	self.metadata = metadata;
	return YES;
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	TagLib::FileStream stream(self.url.fileSystemRepresentation);
	if(!stream.isOpen()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be opened for writing.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Input/output error", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been renamed, moved, deleted, or you may not have appropriate permissions.", @"")];
		return NO;
	}

	TagLib::FLAC::File file(&stream);
	if(!file.isValid()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid FLAC file.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Not a FLAC file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	// ID3v1 and ID3v2 tags are only written if present, but a Xiph comment is always written

	if(file.hasID3v1Tag())
		SFB::Audio::SetID3v1TagFromMetadata(self.metadata, file.ID3v1Tag());

	if(file.hasID3v2Tag())
		SFB::Audio::SetID3v2TagFromMetadata(self.metadata, file.ID3v2Tag());

	SFB::Audio::SetXiphCommentFromMetadata(self.metadata, file.xiphComment());

	// Add album art
	for(SFBAttachedPicture *attachedPicture in self.metadata.attachedPictures) {
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

	if(!file.save()) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
											 code:SFBAudioFileErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” could not be saved.", @"")
											  url:self.url
									failureReason:NSLocalizedString(@"Unable to write metadata", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}

	return YES;
}

@end
