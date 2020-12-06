/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import "SFBDoPDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "NSError+SFBURLPresentation.h"
#import "SFBAudioDecoder+Internal.h"
#import "SFBDSDDecoder.h"

#define DSD_PACKETS_PER_DOP_FRAME (16 / SFB_PCM_FRAMES_PER_DSD_PACKET)
#define BUFFER_SIZE_PACKETS 4096

// Bit reversal lookup table from http://graphics.stanford.edu/~seander/bithacks.html#BitReverseTable
static const uint8_t sBitReverseTable256 [256] =
{
#   define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#   define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#   define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
	R6(0), R6(2), R6(1), R6(3)
};

// Support DSD64, DSD128, and DSD256 (64x, 128x, and 256x the CD sample rate of 44.1 KHz)
// as well as the 48.0 KHz variants 6.144 MHz and 12.288 MHz
static BOOL IsSupportedDoPSampleRate(Float64 sampleRate)
{
	if(sampleRate == SFBDSDSampleRateDSD64)
		return YES;
	else if(sampleRate == SFBDSDSampleRateDSD128)
		return YES;
	else if(sampleRate == SFBDSDSampleRateDSD256)
		return YES;
	else if(sampleRate == SFBDSDSampleRateVariantDSD128)
		return YES;
	else if(sampleRate == SFBDSDSampleRateVariantDSD256)
		return YES;
	else
		return NO;
}

@interface SFBDoPDecoder ()
{
@private
	id <SFBDSDDecoding> _decoder;
	AVAudioFormat *_processingFormat;
	AVAudioCompressedBuffer *_buffer;
	uint8_t _marker;
	BOOL _reverseBits;
}
@end

@implementation SFBDoPDecoder

@synthesize processingFormat = _processingFormat;

- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource)
		return nil;
	return [self initWithInputSource:inputSource error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);

	SFBDSDDecoder *decoder = [[SFBDSDDecoder alloc] initWithInputSource:inputSource error:error];
	if(!decoder)
		return nil;

	return [self initWithDecoder:decoder error:error];
}

- (instancetype)initWithDecoder:(id <SFBDSDDecoding>)decoder error:(NSError **)error
{
	NSParameterAssert(decoder != nil);

	if((self = [super init])) {
		_decoder = decoder;
		_marker = 0x05;
	}
	return self;
}

- (SFBInputSource *)inputSource
{
	return _decoder.inputSource;
}

- (AVAudioFormat *)sourceFormat
{
	return _decoder.sourceFormat;
}

- (BOOL)decodingIsLossless
{
	return _decoder.decodingIsLossless;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(!_decoder.isOpen && ![_decoder openReturningError:error])
		return NO;

	const AudioStreamBasicDescription *asbd = _decoder.processingFormat.streamDescription;

	if(!(asbd->mFormatID == SFBAudioFormatIDDirectStreamDigital)) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBDSDDecoderErrorDomain
											 code:SFBDSDDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid DSD file.", @"")
											  url:_decoder.inputSource.url
									failureReason:NSLocalizedString(@"Not a DSD file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	if(!IsSupportedDoPSampleRate(asbd->mSampleRate)) {
		os_log_error(gSFBAudioDecoderLog, "Unsupported DSD sample rate for DoP: %f", asbd->mSampleRate);
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBDSDDecoderErrorDomain
											 code:SFBDSDDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not supported.", @"")
											  url:_decoder.inputSource.url
									failureReason:NSLocalizedString(@"Unsupported DSD sample rate", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's sample rate is not supported for DSD over PCM.", @"")];

		return NO;
	}

	_reverseBits = !(asbd->mFormatFlags & kAudioFormatFlagIsBigEndian);

	// Generate non-interleaved 24-bit big endian output
	AudioStreamBasicDescription processingStreamDescription = {0};

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM/*SFBAudioFormatIDDoP*/;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsBigEndian;

	processingStreamDescription.mSampleRate			= asbd->mSampleRate / (SFB_PCM_FRAMES_PER_DSD_PACKET * DSD_PACKETS_PER_DOP_FRAME);
	processingStreamDescription.mChannelsPerFrame	= asbd->mChannelsPerFrame;
	processingStreamDescription.mBitsPerChannel		= 24;

	processingStreamDescription.mBytesPerPacket		= 3;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket / processingStreamDescription.mFramesPerPacket;

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:_decoder.processingFormat.channelLayout];

	_buffer = [[AVAudioCompressedBuffer alloc] initWithFormat:_decoder.processingFormat packetCapacity:BUFFER_SIZE_PACKETS maximumPacketSize:(SFB_BYTES_PER_DSD_PACKET_PER_CHANNEL * _decoder.processingFormat.channelCount)];
	_buffer.packetCount = 0;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_buffer = nil;
	return [_decoder closeReturningError:error];
}

- (BOOL)isOpen
{
	return _buffer != nil;
}

- (AVAudioFramePosition)framePosition
{
	return _decoder.packetPosition / DSD_PACKETS_PER_DOP_FRAME;
}

- (AVAudioFramePosition)frameLength
{
	return _decoder.packetCount / DSD_PACKETS_PER_DOP_FRAME;
}

- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error {
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer isKindOfClass:[AVAudioPCMBuffer class]]);
	return [self decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:((AVAudioPCMBuffer *)buffer).frameCapacity error:error];
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	if(frameLength == 0)
		return YES;

	AVAudioFrameCount framesRead = 0;

	for(;;) {
		AVAudioFrameCount framesRemaining = frameLength - framesRead;

		// Grab the DSD audio
		AVAudioPacketCount dsdPacketsRemaining = framesRemaining * DSD_PACKETS_PER_DOP_FRAME;
		if(![_decoder decodeIntoBuffer:_buffer packetCount:MIN(_buffer.packetCapacity, dsdPacketsRemaining) error:error])
			break;

		AVAudioPacketCount dsdPacketsDecoded = _buffer.packetCount;
		if(dsdPacketsDecoded == 0)
			break;

		// Convert to DoP
		// NB: Currently DSDIFFDecoder and DSFDecoder only produce interleaved output

		AVAudioFrameCount framesDecoded = dsdPacketsDecoded / DSD_PACKETS_PER_DOP_FRAME;

		uint8_t marker = _marker;
		AVAudioChannelCount channelCount = _processingFormat.channelCount;
		for(AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
			const uint8_t *input = (uint8_t *)_buffer.data + channel;
			uint8_t *output = (uint8_t *)buffer.audioBufferList->mBuffers[channel].mData + buffer.audioBufferList->mBuffers[channel].mDataByteSize;

			// The DoP marker should match across channels
			marker = _marker;
			for(AVAudioFrameCount i = 0; i < framesDecoded; ++i) {
				// Insert the DoP marker
				*output++ = marker;

				// Copy the DSD bits
				*output++ = _reverseBits ? sBitReverseTable256[*input] : *input;
				input += channelCount;
				*output++ = _reverseBits ? sBitReverseTable256[*input] : *input;
				input += channelCount;

				marker = marker == (uint8_t)0x05 ? (uint8_t)0xfa : (uint8_t)0x05;
			}
		}

		_marker = marker;

		buffer.frameLength += framesDecoded;
		framesRead += framesDecoded;

		// All requested frames were read
		if(framesRead == frameLength)
			break;
	}

	return YES;
}

- (BOOL)supportsSeeking
{
	return _decoder.supportsSeeking;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	if(![_decoder seekToPacket:(frame * DSD_PACKETS_PER_DOP_FRAME) error:error])
		return NO;

	_buffer.packetCount = 0;
	_buffer.byteLength = 0;

	return YES;
}

@end
