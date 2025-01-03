//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

@import AVFAudioExtensions;

#import "SFBAudioExporter.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"

// NSError domain for SFBAudioExporter
NSErrorDomain const SFBAudioExporterErrorDomain = @"org.sbooth.AudioEngine.AudioExporter";

#define BUFFER_SIZE_FRAMES 2048

@implementation SFBAudioExporter

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBAudioExporterErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
			switch(err.code) {
				case SFBAudioExporterErrorCodeFileFormatNotSupported:
					return NSLocalizedString(@"The file's format is not supported.", @"");
			}
		}
		return nil;
	}];
}

+ (BOOL)exportFromURL:(NSURL *)sourceURL toURL:(NSURL *)targetURL error:(NSError **)error
{
	NSParameterAssert(sourceURL != nil);
	NSParameterAssert(targetURL != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:sourceURL error:error];
	if(!decoder)
		return NO;
	return [self exportFromDecoder:decoder toURL:targetURL error:error];
}

+ (BOOL)exportFromDecoder:(id<SFBPCMDecoding>)decoder toURL:(NSURL *)targetURL error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(targetURL != nil);

	void(^deleteOutputFile)(void) = ^{
#if TARGET_OS_TV
		[[NSFileManager defaultManager] removeItemAtURL:targetURL error:nil];
#else
		[[NSFileManager defaultManager] trashItemAtURL:targetURL resultingItemURL:nil error:nil];
#endif
	};

	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;

	AVAudioFormat *processingFormat = decoder.processingFormat.standardEquivalent;

	AVAudioConverter *converter = [[AVAudioConverter alloc] initFromFormat:decoder.processingFormat toFormat:processingFormat];
	if(!converter) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioExporterErrorDomain
											 code:SFBAudioExporterErrorCodeFileFormatNotSupported
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” is not supported.", @"")
											  url:decoder.inputSource.url
									failureReason:NSLocalizedString(@"Unsupported file format", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's format is not supported for export.", @"")];
		return NO;
	}

	AVAudioFile *outputFile = [[AVAudioFile alloc] initForWriting:targetURL settings:decoder.processingFormat.settings commonFormat:processingFormat.commonFormat interleaved:processingFormat.interleaved error:error];
	if(!outputFile)
		return NO;

	AVAudioPCMBuffer *outputBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.outputFormat frameCapacity:BUFFER_SIZE_FRAMES];
	AVAudioPCMBuffer *decodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.inputFormat frameCapacity:BUFFER_SIZE_FRAMES];

	__block BOOL decodeResult;
	__block NSError *decodeError = nil;
	NSError *convertError = nil;
	NSError *writeError = nil;

	for(;;) {
		AVAudioConverterOutputStatus status = [converter convertToBuffer:outputBuffer error:&convertError withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *outStatus) {
			decodeResult = [decoder decodeIntoBuffer:decodeBuffer frameLength:inNumberOfPackets error:&decodeError];
			if(!decodeResult) {
				*outStatus = AVAudioConverterInputStatus_NoDataNow;
				return nil;
			}

			if(decodeBuffer.frameLength == 0) {
				*outStatus = AVAudioConverterInputStatus_EndOfStream;
				return nil;
			}

			*outStatus = AVAudioConverterInputStatus_HaveData;
			return decodeBuffer;
		}];

		if(!decodeResult) {
			os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", decodeError);
			if(error)
				*error = decodeError;
			deleteOutputFile();
			return NO;
		}
		if(status == AVAudioConverterOutputStatus_Error) {
			os_log_error(OS_LOG_DEFAULT, "Error converting PCM audio: %{public}@", convertError);
			if(error)
				*error = convertError;
			deleteOutputFile();
			return NO;
		}
		else if(status == AVAudioConverterOutputStatus_EndOfStream)
			break;

		if(![outputFile writeFromBuffer:outputBuffer error:&writeError]) {
			os_log_error(OS_LOG_DEFAULT, "Error writing audio: %{public}@", writeError);
			if(error)
				*error = writeError;
			deleteOutputFile();
			return NO;
		}
	}

	return YES;
}

@end
