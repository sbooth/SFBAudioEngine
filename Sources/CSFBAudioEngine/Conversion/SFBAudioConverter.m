//
// Copyright (c) 2020-2025 Stephen F. Booth <me@sbooth.org>
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

@implementation SFBAudioConverter

+ (void)load
{
	[NSError setUserInfoValueProviderForDomain:SFBAudioConverterErrorDomain provider:^id(NSError *err, NSErrorUserInfoKey userInfoKey) {
		if([userInfoKey isEqualToString:NSLocalizedDescriptionKey]) {
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
	return [converter convertReturningError:error];
}

+ (BOOL)convertFromURL:(NSURL *)sourceURL usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] initWithURL:sourceURL encoder:encoder error:error];
	return [converter convertReturningError:error];
}

+ (BOOL)convertFromDecoder:(id <SFBPCMDecoding>)decoder toURL:(NSURL *)destinationURL error:(NSError **)error
{
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] initWithDecoder:decoder destinationURL:destinationURL error:error];
	return [converter convertReturningError:error];
}

+ (BOOL)convertFromDecoder:(id <SFBPCMDecoding>)decoder usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] initWithDecoder:decoder encoder:encoder error:error];
	return [converter convertReturningError:error];
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
	return [self initWithDecoder:decoder encoder:encoder requestedIntermediateFormat:nil error:error];
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
	return [self initWithDecoder:decoder encoder:encoder requestedIntermediateFormat:nil error:error];
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
	return [self initWithDecoder:decoder encoder:encoder requestedIntermediateFormat:nil error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder
{
	return [self initWithDecoder:decoder encoder:encoder requestedIntermediateFormat:nil error:nil];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	return [self initWithDecoder:decoder encoder:encoder requestedIntermediateFormat:nil error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder requestedIntermediateFormat:(AVAudioFormat *(^)(AVAudioFormat *))intermediateFormatBlock error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(encoder != nil);

	if((self = [super init])) {
		if(!decoder.isOpen && ![decoder openReturningError:error])
			return nil;
		_decoder = decoder;

		if(!encoder.isOpen) {
			AVAudioFormat *desiredIntermediateFormat = decoder.processingFormat;

			// Encode lossy sources as 16-bit PCM
			if(!decoder.decodingIsLossless) {
				AVAudioChannelLayout *decoderChannelLayout = decoder.processingFormat.channelLayout;
				if(decoderChannelLayout)
					desiredIntermediateFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:decoder.processingFormat.sampleRate interleaved:YES channelLayout:decoderChannelLayout];
				else
					desiredIntermediateFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:decoder.processingFormat.sampleRate channels:decoder.processingFormat.channelCount interleaved:YES];
			}

			if(intermediateFormatBlock)
				desiredIntermediateFormat = intermediateFormatBlock(desiredIntermediateFormat);

			if(![encoder setSourceFormat:desiredIntermediateFormat error:error])
				return nil;

			encoder.estimatedFramesToEncode = decoder.frameLength;

			if(![encoder openReturningError:error])
				return nil;
		}

		_encoder = encoder;

		_intermediateConverter = [[AVAudioConverter alloc] initFromFormat:decoder.processingFormat toFormat:encoder.processingFormat];
		if(!_intermediateConverter) {
			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioConverterErrorDomain
												 code:SFBAudioConverterErrorCodeFormatNotSupported
						descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” is not supported.", @"")
												  url:decoder.inputSource.url
										failureReason:NSLocalizedString(@"Unsupported file format", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's format is not supported for conversion.", @"")];
			return nil;
		}
	}
	return self;
}

- (BOOL)convertReturningError:(NSError **)error
{
	AVAudioPCMBuffer *encodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_intermediateConverter.outputFormat frameCapacity:BUFFER_SIZE_FRAMES];
	AVAudioPCMBuffer *decodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_intermediateConverter.inputFormat frameCapacity:BUFFER_SIZE_FRAMES];

	__block BOOL decodeResult;
	__block NSError *decodeError = nil;
	NSError *convertError = nil;
	NSError *encodeError = nil;

	for(;;) {
		AVAudioConverterOutputStatus status = [_intermediateConverter convertToBuffer:encodeBuffer error:&convertError withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *outStatus) {
			decodeResult = [self->_decoder decodeIntoBuffer:decodeBuffer frameLength:inNumberOfPackets error:&decodeError];
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

		// Verify decoding was successful
		if(!decodeResult) {
			os_log_error(OS_LOG_DEFAULT, "Error decoding audio: %{public}@", decodeError);
			if(error)
				*error = decodeError;
			return NO;
		}

		// Check conversion status
		if(status == AVAudioConverterOutputStatus_Error) {
			os_log_error(OS_LOG_DEFAULT, "Error converting PCM audio: %{public}@", convertError);
			if(error)
				*error = convertError;
			return NO;
		}
		else if(status == AVAudioConverterOutputStatus_EndOfStream)
			break;

		// Send converted data to the encoder
		if(![_encoder encodeFromBuffer:encodeBuffer frameLength:encodeBuffer.frameLength error:&encodeError]) {
			os_log_error(OS_LOG_DEFAULT, "Error encoding audio: %{public}@", encodeError);
			if(error)
				*error = encodeError;
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
