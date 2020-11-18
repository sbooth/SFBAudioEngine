/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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

+ (BOOL)convertURL:(NSURL *)sourceURL toURL:(NSURL *)destinationURL error:(NSError **)error
{
	SFBAudioConverter *converter = [[SFBAudioConverter alloc] initWithURL:sourceURL destinationURL:destinationURL error:error];
	return [converter convertReturningError:error];
}

+ (BOOL)convertAudioFromDecoder:(id <SFBPCMDecoding>)decoder usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
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
	SFBAudioFile *audioFile = [SFBAudioFile audioFileWithURL:sourceURL error:nil];
	return [self initWithDecoder:decoder encoder:encoder metadata:audioFile.metadata error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder
{
	return [self initWithDecoder:decoder encoder:encoder error:nil];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	return [self initWithDecoder:decoder encoder:encoder metadata:nil error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder metadata:(SFBAudioMetadata *)metadata
{
	return [self initWithDecoder:decoder encoder:encoder metadata:metadata error:nil];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder encoder:(id <SFBPCMEncoding>)encoder metadata:(SFBAudioMetadata *)metadata error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(encoder != nil);

	if((self = [super init])) {
		if(!decoder.isOpen && ![decoder openReturningError:error])
			return nil;
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

			if(![encoder setSourceFormat:desiredEncodingFormat error:error])
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

		_metadata = [metadata copy];
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
