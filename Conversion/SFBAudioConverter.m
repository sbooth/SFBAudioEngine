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
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] initWithURL:sourceURL destinationURL:destinationURL error:error];
	if(![converter convertReturningError:error])
		return NO;

	// Silently fail if metadata can't be read or written
	[SFBAudioFile copyMetadataFromURL:sourceURL toURL:destinationURL error:nil];
	return YES;
}

+ (BOOL)convertFromURL:(NSURL *)sourceURL usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] initWithURL:sourceURL encoder:encoder error:error];
	if(![converter convertReturningError:error])
		return NO;

	// Silently fail if metadata can't be read or written
	if(converter.encoder.outputSource.url.isFileURL)
		[SFBAudioFile copyMetadataFromURL:sourceURL toURL:converter.encoder.outputSource.url error:nil];
	return YES;
}

+ (BOOL)convertFromDecoder:(id <SFBPCMDecoding>)decoder toURL:(NSURL *)destinationURL error:(NSError **)error
{
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] initWithDecoder:decoder destinationURL:destinationURL error:error];
	if(![converter convertReturningError:error])
		return NO;

	// Silently fail if metadata can't be read or written
	if(converter.decoder.inputSource.url.isFileURL)
		[SFBAudioFile copyMetadataFromURL:converter.decoder.inputSource.url toURL:destinationURL error:nil];
	return YES;
}

+ (BOOL)convertFromDecoder:(id <SFBPCMDecoding>)decoder usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] initWithDecoder:decoder encoder:encoder error:error];
	if(![converter convertReturningError:error])
		return NO;

	// Silently fail if metadata can't be read or written
	if(converter.decoder.inputSource.url.isFileURL && converter.encoder.outputSource.url.isFileURL)
		[SFBAudioFile copyMetadataFromURL:converter.decoder.inputSource.url toURL:converter.encoder.outputSource.url error:nil];
	return YES;
}

- (instancetype)initWithURL:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL
{
	return [self initWithURL:sourceURL destinationURL:destinationURL error:nil];
}

- (instancetype)initWithURL:(NSURL *)sourceURL destinationURL:(NSURL *)destinationURL error:(NSError **)error
{
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:sourceURL error:error];
	if(!decoder)
		return nil;
	SFBAudioEncoder *encoder = [[SFBAudioEncoder alloc] initWithURL:destinationURL error:error];
	if(!encoder)
		return nil;
	return [self initWithDecoder:decoder encoder:encoder requestedProcessingFormat:nil intermediateConverter:nil error:error];
}

- (instancetype)initWithURL:(NSURL *)sourceURL encoder:(id <SFBPCMEncoding>)encoder
{
	return [self initWithURL:sourceURL encoder:encoder error:nil];
}

- (instancetype)initWithURL:(NSURL *)sourceURL encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:sourceURL error:error];
	if(!decoder)
		return nil;
	return [self initWithDecoder:decoder encoder:encoder requestedProcessingFormat:nil intermediateConverter:nil error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder destinationURL:(NSURL *)destinationURL
{
	return [self initWithDecoder:decoder destinationURL:destinationURL error:nil];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder destinationURL:(NSURL *)destinationURL error:(NSError **)error
{
	SFBAudioEncoder *encoder = [[SFBAudioEncoder alloc] initWithURL:destinationURL error:error];
	if(!encoder)
		return nil;
	return [self initWithDecoder:decoder encoder:encoder requestedProcessingFormat:nil intermediateConverter:nil error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder
{
	return [self initWithDecoder:decoder encoder:encoder requestedProcessingFormat:nil intermediateConverter:nil error:nil];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	return [self initWithDecoder:decoder encoder:encoder requestedProcessingFormat:nil intermediateConverter:nil error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder requestedProcessingFormat:(AVAudioFormat *(^)(AVAudioFormat *))processingFormatBlock intermediateConverter:(void(^)(AVAudioConverter *))converterCustomizationBlock error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(encoder != nil);

	if((self = [super init])) {
		if(!decoder.isOpen && ![decoder openReturningError:error])
			return nil;
		_decoder = decoder;

		if(!encoder.isOpen) {
			AVAudioFormat *desiredProcessingFormat = decoder.processingFormat;

			// Encode lossy sources as 16-bit PCM
			if(!decoder.decodingIsLossless) {
				AVAudioChannelLayout *decoderChannelLayout = decoder.processingFormat.channelLayout;
				if(decoderChannelLayout)
					desiredProcessingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:decoder.processingFormat.sampleRate interleaved:YES channelLayout:decoderChannelLayout];
				else
					desiredProcessingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:decoder.processingFormat.sampleRate channels:decoder.processingFormat.channelCount interleaved:YES];
			}

			if(processingFormatBlock)
				desiredProcessingFormat = processingFormatBlock(desiredProcessingFormat);

			if(![encoder setSourceFormat:desiredProcessingFormat error:error])
				return nil;

			encoder.estimatedFramesToEncode = decoder.frameLength;

			if(![encoder openReturningError:error])
				return nil;
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
			return nil;
		}

		if(converterCustomizationBlock)
			converterCustomizationBlock(_converter);
	}
	return self;
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

	return [_decoder closeReturningError:error];
}

@end
