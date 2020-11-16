/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBAudioConverter.h"

#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder.h"
#import "SFBAudioEncoder.h"

// NSError domain for SFBAudioConverter
NSErrorDomain const SFBAudioConverterErrorDomain = @"org.sbooth.AudioEngine.AudioConverter";

#define BUFFER_SIZE_FRAMES 2048

@implementation SFBAudioConverter

+ (BOOL)convertURL:(NSURL *)sourceURL toURL:(NSURL *)targetURL error:(NSError **)error
{
	NSParameterAssert(sourceURL != nil);
	NSParameterAssert(targetURL != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithURL:sourceURL error:error];
	if(!decoder)
		return NO;
	SFBAudioEncoder *encoder = [[SFBAudioEncoder alloc] initWithURL:targetURL error:error];
	if(!encoder)
		return NO;
	return [self convertAudioFromDecoder:decoder usingEncoder:encoder error:error];
}

+ (BOOL)convertAudioFromDecoder:(id <SFBPCMDecoding>)decoder usingEncoder:(id <SFBPCMEncoding>)encoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(encoder != nil);

	if(!decoder.isOpen && ![decoder openReturningError:error])
		return NO;

	if(!encoder.isOpen) {
		AVAudioFormat *sourceFormat = decoder.sourceFormat;
		AVAudioFormat *desiredEncodingFormat = decoder.processingFormat;

		// Encode lossy sources as 16-bit PCM
		if(sourceFormat.streamDescription->mBitsPerChannel == 0) {
			AVAudioChannelLayout *decoderChannelLayout = decoder.processingFormat.channelLayout;
			if(decoderChannelLayout)
				desiredEncodingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:decoder.processingFormat.sampleRate interleaved:YES channelLayout:decoderChannelLayout];
			else
				desiredEncodingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:decoder.processingFormat.sampleRate channels:decoder.processingFormat.channelCount interleaved:YES];
		}

		if(![encoder openWithSourceFormat:desiredEncodingFormat error:error])
			return NO;
	}

	AVAudioConverter *converter = [[AVAudioConverter alloc] initFromFormat:decoder.processingFormat toFormat:encoder.processingFormat];
	if(!converter) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioConverterErrorDomain
											 code:SFBAudioConverterErrorCodeFormatNotSupported
					descriptionFormatStringForURL:NSLocalizedString(@"The format of the file “%@” is not supported.", @"")
											  url:decoder.inputSource.url
									failureReason:NSLocalizedString(@"Unsupported file format", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's format is not supported for conversion.", @"")];
		return NO;
	}

	AVAudioPCMBuffer *encodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.outputFormat frameCapacity:BUFFER_SIZE_FRAMES];
	AVAudioPCMBuffer *decodeBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:converter.inputFormat frameCapacity:BUFFER_SIZE_FRAMES];

	for(;;) {
		__block NSError *err = nil;
		AVAudioConverterOutputStatus status = [converter convertToBuffer:encodeBuffer error:error withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus *outStatus) {
			BOOL result = [decoder decodeIntoBuffer:decodeBuffer frameLength:inNumberOfPackets error:&err];
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
			if(error)
				*error = err;
			return NO;
		}
		else if(status == AVAudioConverterOutputStatus_EndOfStream)
			break;

		if(![encoder encodeFromBuffer:encodeBuffer frameLength:encodeBuffer.frameLength error:&err]) {
			os_log_error(OS_LOG_DEFAULT, "Error encoding audio: %{public}@", err);
			if(error)
				*error = err;
			return NO;
		}
	}

	return YES;
}

@end
