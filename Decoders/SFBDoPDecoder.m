/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBDoPDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "NSError+SFBURLPresentation.h"
#import "SFBDSDDecoder.h"

#define DSD_PACKETS_PER_DOP_FRAME 16
#define BUFFER_SIZE_PACKETS 4096

static inline AVAudioFrameCount SFB_min(AVAudioFrameCount a, AVAudioFrameCount b) { return a < b ? a : b; }

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
	if(sampleRate == 2822400)
		return YES;
	else if(sampleRate == 5644800)
		return YES;
	else if(sampleRate == 11289600)
		return YES;
	else if(sampleRate == 6144000)
		return YES;
	else if(sampleRate == 12288000)
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
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
	uint8_t _marker;
	BOOL _reverseBits;
}
@end

@implementation SFBDoPDecoder

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

	if((self = [super init]))
		_decoder = decoder;
	return self;
}

- (SFBInputSource *)inputSource
{
	return _decoder.inputSource;
}

- (AVAudioFormat *)processingFormat
{
	return  _processingFormat;
}

- (AVAudioFormat *)sourceFormat
{
	return _decoder.sourceFormat;
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
		os_log_error(OS_LOG_DEFAULT, "Unsupported DSD sample rate for DoP: %f", asbd->mSampleRate);
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

	// Generate interleaved 24-bit big endian output
	AudioStreamBasicDescription processingStreamDescription = {0};

	processingStreamDescription.mFormatID			= SFBAudioFormatIDDoP;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagIsBigEndian;

	processingStreamDescription.mSampleRate			= asbd->mSampleRate / DSD_PACKETS_PER_DOP_FRAME;
	processingStreamDescription.mChannelsPerFrame	= asbd->mChannelsPerFrame;
	processingStreamDescription.mBitsPerChannel		= 24;

	processingStreamDescription.mBytesPerPacket		= 3 * processingStreamDescription.mChannelsPerFrame;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket * processingStreamDescription.mFramesPerPacket;

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:_decoder.processingFormat.channelLayout];

	_buffer = [[AVAudioCompressedBuffer alloc] initWithFormat:_decoder.processingFormat packetCapacity:BUFFER_SIZE_PACKETS];

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
	if(![buffer isKindOfClass:[AVAudioPCMBuffer class]])
		return NO;
	return [self decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:((AVAudioPCMBuffer *)buffer).frameCapacity error:error];
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(OS_LOG_DEFAULT, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	AVAudioFrameCount framesRead = 0;

	for(;;) {
		AVAudioFrameCount framesRemaining = frameLength - framesRead;

		// Grab the DSD audio
		_buffer.packetCount = 0;
		AVAudioFrameCount dsdPacketsRemaining = framesRemaining * DSD_PACKETS_PER_DOP_FRAME;
		if(![_decoder decodeIntoBuffer:_buffer packetCount:SFB_min(_buffer.packetCapacity, dsdPacketsRemaining) error:error])
			break;

		AVAudioFrameCount dsdPacketsDecoded = _buffer.packetCount;
		if(dsdPacketsDecoded == 0)
			break;

		AVAudioFrameCount framesDecoded = dsdPacketsDecoded / DSD_PACKETS_PER_DOP_FRAME;

		// Convert to DoP
		// NB: Currently DSDIFFDecoder and DSFDecoder only produce interleaved output

		const uint8_t *src = (const uint8_t *)_buffer.data;
		uint8_t *dst = (uint8_t *)buffer.audioBufferList->mBuffers[0].mData + buffer.audioBufferList->mBuffers[0].mDataByteSize;

		for(AVAudioFrameCount i = 0; i < framesDecoded; ++i) {
			// Insert the DSD marker
			*dst++ = _marker;

			// Copy the DSD bits
			if(_reverseBits) {
				*dst++ = sBitReverseTable256[*src++];
				*dst++ = sBitReverseTable256[*src++];
			}
			else {
				*dst++ = *src++;
				*dst++ = *src++;
			}

			_marker = _marker == (uint8_t)0x05 ? (uint8_t)0xfa : (uint8_t)0x05;
		}

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

	return YES;
}

@end
