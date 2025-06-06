//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <taglib/tfilestream.h>
#import <taglib/wavpackfile.h>

#import "SFBWavPackFile.h"

#import "AddAudioPropertiesToDictionary.h"
#import "NSData+SFBExtensions.h"
#import "NSError+SFBURLPresentation.h"
#import "NSFileHandle+SFBHeaderReading.h"
#import "SFBAudioMetadata+TagLibAPETag.h"
#import "SFBAudioMetadata+TagLibID3v1Tag.h"

SFBAudioFileFormatName const SFBAudioFileFormatNameWavPack = @"org.sbooth.AudioEngine.File.WavPack";

@implementation SFBWavPackFile

+ (void)load
{
	[SFBAudioFile registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"wv"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/wavpack", @"audio/x-wavpack"]];
}

+ (SFBAudioFileFormatName)formatName
{
	return SFBAudioFileFormatNameWavPack;
}

+ (BOOL)testFileHandle:(NSFileHandle *)fileHandle formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(fileHandle != nil);
	NSParameterAssert(formatIsSupported != NULL);

	NSData *header = [fileHandle readHeaderOfLength:SFBWavPackDetectionSize skipID3v2Tag:NO error:error];
	if(!header)
		return NO;

	if([header isWavPackHeader])
		*formatIsSupported = SFBTernaryTruthValueTrue;
	else
		*formatIsSupported = SFBTernaryTruthValueFalse;

	return YES;
}

- (BOOL)readPropertiesAndMetadataReturningError:(NSError **)error
{
	try {
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

		TagLib::WavPack::File file(&stream);
		if(!file.isValid()) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
												 code:SFBAudioFileErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WavPack file.", @"")
												  url:self.url
										failureReason:NSLocalizedString(@"Not a WavPack file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		NSMutableDictionary *propertiesDictionary = [NSMutableDictionary dictionaryWithObject:@"WavPack" forKey:SFBAudioPropertiesKeyFormatName];
		if(file.audioProperties()) {
			auto properties = file.audioProperties();
			SFB::Audio::AddAudioPropertiesToDictionary(properties, propertiesDictionary);

			if(properties->bitsPerSample())
				propertiesDictionary[SFBAudioPropertiesKeyBitDepth] = @(properties->bitsPerSample());
			if(properties->sampleFrames())
				propertiesDictionary[SFBAudioPropertiesKeyFrameLength] = @(properties->sampleFrames());
		}

		SFBAudioMetadata *metadata = [[SFBAudioMetadata alloc] init];
		if(file.hasID3v1Tag())
			[metadata addMetadataFromTagLibID3v1Tag:file.ID3v1Tag()];

		if(file.hasAPETag())
			[metadata addMetadataFromTagLibAPETag:file.APETag()];

		self.properties = [[SFBAudioProperties alloc] initWithDictionaryRepresentation:propertiesDictionary];
		self.metadata = metadata;

		return YES;
	}
	catch(const std::exception& e) {
		os_log_error(gSFBAudioFileLog, "Error reading WavPack properties and metadata: %{public}s", e.what());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioFileErrorDomain code:SFBAudioFileErrorCodeInternalError userInfo:nil];
		return NO;
	}
}

- (BOOL)writeMetadataReturningError:(NSError **)error
{
	try {
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

		TagLib::WavPack::File file(&stream, false);
		if(!file.isValid()) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioFileErrorDomain
												 code:SFBAudioFileErrorCodeInvalidFormat
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WavPack file.", @"")
												  url:self.url
										failureReason:NSLocalizedString(@"Not a WavPack file", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
			return NO;
		}

		// ID3v1 tags are only written if present, but an APE tag is always written

		if(file.hasID3v1Tag())
			SFB::Audio::SetID3v1TagFromMetadata(self.metadata, file.ID3v1Tag());

		SFB::Audio::SetAPETagFromMetadata(self.metadata, file.APETag(true));

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
	catch(const std::exception& e) {
		os_log_error(gSFBAudioFileLog, "Error writing WavPack metadata: %{public}s", e.what());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioFileErrorDomain code:SFBAudioFileErrorCodeInternalError userInfo:nil];
		return NO;
	}
}

@end
