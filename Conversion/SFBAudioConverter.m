//
// Copyright (c) 2020 - 2022 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

#import "SFBAudioConverter.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"
#import "SFBAudioEncoder.h"
#import "SFBAudioFile.h"

// NSError domain for SFBAudioConverter
NSErrorDomain const SFBAudioConverterErrorDomain = @"org.sbooth.AudioEngine.AudioConverter";

#define BUFFER_SIZE_FRAMES 2048

@interface SFBAudioConverter ()
{
@private
	AVAudioConverter *_converter;
}
@end

@implementation SFBAudioConverter

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBAudioConverterErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if(userInfoKey == NSLocalizedDescriptionKey) {
			switch(err.code) {
				case SFBAudioConverterErrorCodeFormatNotSupported:
					return NSLocalizedString(@"The requested audio format is not supported.", @"");
			}
		}
		return nil;
	}];
}

+ (BOOL)convertFromURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error
{
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:sourceURL error:error];
	if(!decoder)
		return NO;

	SFBAudioEncoder *encoder = [[SFBAudioEncoder alloc] initWithURL:destinationURL error:error];
	if(!encoder)
		return NO;

	SFBAudioConverter *converter = [[SFBAudioConverter alloc] init];

	// Silently fail if metadata can't be read
	SFBAudioFile *audioFile = [SFBAudioFile audioFileWithURL:sourceURL error:nil];
	converter.metadata = audioFile.metadata;

	if(![converter setDecoder:decoder encoder:encoder error:error])
		return NO;
	return [converter convertReturningError:error];
}

+ (BOOL)convertFromURL:(NSURL *)sourceURL usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:sourceURL error:error];
	if(!decoder)
		return NO;

	SFBAudioConverter *converter = [[SFBAudioConverter alloc] init];

	// Silently fail if metadata can't be read
	SFBAudioFile *audioFile = [SFBAudioFile audioFileWithURL:sourceURL error:nil];
	converter.metadata = audioFile.metadata;

	if(![converter setDecoder:decoder encoder:encoder error:error])
		return NO;
	return [converter convertReturningError:error];
}

+ (BOOL)convertFromDecoder:(id <SFBPCMDecoding>)decoder toURL:(NSURL *)destinationURL error:(NSError **)error
{
	SFBAudioEncoder *encoder = [[SFBAudioEncoder alloc] initWithURL:destinationURL error:error];
	if(!encoder)
		return NO;

	SFBAudioConverter *converter = [[SFBAudioConverter alloc] init];
	if(![converter setDecoder:decoder encoder:encoder error:error])
		return NO;
	return [converter convertReturningError:error];
}

+ (BOOL)convertFromDecoder:(id <SFBPCMDecoding>)decoder usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] init];
	if(![converter setDecoder:decoder encoder:encoder error:error])
		return NO;
	return [converter convertReturningError:error];
}

- (BOOL)setDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(encoder != nil);

	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;
	_decoder = decoder;

	if(!encoder.isOpen) {
		AVAudioFormat *desiredEncodingFormat = decoder.processingFormat;

		// Encode lossy sources as 16-bit PCM
		if(!decoder.decodingIsLossless) {
			AVAudioChannelLayout *decoderChannelLayout = decoder.processingFormat.channelLayout;
			if(decoderChannelLayout)
				desiredEncodingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:decoder.processingFormat.sampleRate interleaved:YES channelLayout:decoderChannelLayout];
			else
				desiredEncodingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:decoder.processingFormat.sampleRate channels:decoder.processingFormat.channelCount interleaved:YES];
		}

		if([_delegate respondsToSelector:@selector(audioConverter:proposedProcessingFormatForConversion:)])
			desiredEncodingFormat = [_delegate audioConverter:self proposedProcessingFormatForConversion:desiredEncodingFormat];

		if(![encoder setSourceFormat:desiredEncodingFormat error:error])
			return NO;

		encoder.estimatedFramesToEncode = decoder.frameLength;

		if(![encoder openReturningError:error])
			return NO;
	}
	_encoder = encoder;

	_converter = [[AVAudioConverter alloc] initFromFormat:decoder.processingFormat toFormat:encoder.processingFormat];
	if(!_converter) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioConverterErrorDomain
											 code:SFBAudioConverterErrorCodeFormatNotSupported
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” is not supported.", @"")
											  url:decoder.inputSource.url
									failureReason:NSLocalizedString(@"Unsupported file format", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's format is not supported for conversion.", @"")];
		return NO;
	}

	if([_delegate respondsToSelector:@selector(audioConverter:customizeConversionParameters:)])
		[_delegate audioConverter:self customizeConversionParameters:_converter];

	return YES;
}

- (BOOL)convertReturningError:(NSError **)error
{
	AVAudioPCMBuffer *encodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_converter.outputFormat frameCapacity:BUFFER_SIZE_FRAMES];
	AVAudioPCMBuffer *decodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_converter.inputFormat frameCapacity:BUFFER_SIZE_FRAMES];

	for(;;) {
		AVAudioConverterOutputStatus status = [_converter convertToBuffer:encodeBuffer error:error withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *outStatus) {
			NSError *err = nil;
			BOOL result = [self->_decoder decodeIntoBuffer:decodeBuffer frameLength:inNumberOfPackets error:&err];
			if(!result)
				os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", err);

			if(decodeBuffer.frameLength == 0) {
				if(result)
					*outStatus = AVAudioConverterInputStatus_EndOfStream;
				else
					*outStatus = AVAudioConverterInputStatus_NoDataNow;
			}
			else
				*outStatus = AVAudioConverterInputStatus_HaveData;

			return decodeBuffer;
		}];

		if(status == AVAudioConverterOutputStatus_Error) {
			return NO;
		}
		else if(status == AVAudioConverterOutputStatus_EndOfStream)
			break;

		if(![_encoder encodeFromBuffer:encodeBuffer frameLength:encodeBuffer.frameLength error:error]) {
			os_log_error(OS_LOG_DEFAULT, "Error encoding audio: %{public}@", error ? *error : nil);
			return NO;
		}
	}

	if(![_encoder finishEncodingReturningError:error])
		return NO;

	if(![_encoder closeReturningError:error])
		return NO;

	if(![_decoder closeReturningError:error])
		return NO;

	if(_metadata && _encoder.outputSource.url.isFileURL) {
		SFBAudioFile *audioFile = [[SFBAudioFile alloc] initWithURL:_encoder.outputSource.url];
		if(audioFile) {
			audioFile.metadata = _metadata;
			if(![audioFile writeMetadataReturningError:error])
				os_log_error(OS_LOG_DEFAULT, "Error writing metadata: %{public}@", error ? *error : nil);
		}
	}

	return YES;
}

@end
