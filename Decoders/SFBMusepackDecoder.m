/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

@import Accelerate;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"

#include <mpc/mpcdec.h>

#pragma clang diagnostic pop

#import "SFBMusepackDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "NSError+SFBURLPresentation.h"

static mpc_int32_t read_callback(mpc_reader *p_reader, void *ptr, mpc_int32_t size)
{
	NSCParameterAssert(p_reader != NULL);

	SFBMusepackDecoder *decoder = (__bridge SFBMusepackDecoder *)p_reader->data;
	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:ptr length:size bytesRead:&bytesRead error:nil])
		return -1;
	return (mpc_int32_t)bytesRead;
}

static mpc_bool_t seek_callback(mpc_reader *p_reader, mpc_int32_t offset)
{
	NSCParameterAssert(p_reader != NULL);

	SFBMusepackDecoder *decoder = (__bridge SFBMusepackDecoder *)p_reader->data;
	return (mpc_bool_t)[decoder->_inputSource seekToOffset:offset error:nil];
}

static mpc_int32_t tell_callback(mpc_reader *p_reader)
{
	NSCParameterAssert(p_reader != NULL);

	SFBMusepackDecoder *decoder = (__bridge SFBMusepackDecoder *)p_reader->data;
	NSInteger offset;
	if(![decoder->_inputSource getOffset:&offset error:nil])
		return -1;
	return (mpc_int32_t)offset;
}

static mpc_int32_t get_size_callback(mpc_reader *p_reader)
{
	NSCParameterAssert(p_reader != NULL);

	SFBMusepackDecoder *decoder = (__bridge SFBMusepackDecoder *)p_reader->data;

	NSInteger length;
	if(![decoder->_inputSource getLength:&length error:nil])
		return -1;
	return (mpc_int32_t)length;
}

static mpc_bool_t canseek_callback(mpc_reader *p_reader)
{
	NSCParameterAssert(p_reader != NULL);

	SFBMusepackDecoder *decoder = (__bridge SFBMusepackDecoder *)p_reader->data;
	return (mpc_bool_t)decoder.supportsSeeking;
}

@interface SFBMusepackDecoder ()
{
@private
	mpc_reader _reader;
	mpc_demux *_demux;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
	AVAudioPCMBuffer *_buffer;
}
@end

@implementation SFBMusepackDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"mpc"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/musepack", @"audio/x-musepack"]];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	_reader.read = read_callback;
	_reader.seek = seek_callback;
	_reader.tell = tell_callback;
	_reader.get_size = get_size_callback;
	_reader.canseek = canseek_callback;
	_reader.data = (__bridge void *)self;

	_demux = mpc_demux_init(&_reader);
	if(!_demux) {
		mpc_reader_exit_stdio(&_reader);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Musepack file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid Musepack file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Get input file information
	mpc_streaminfo streaminfo;
	mpc_demux_get_info(_demux, &streaminfo);

	_frameLength = mpc_streaminfo_get_length_samples(&streaminfo);

	AVAudioChannelLayout *channelLayout = nil;
	switch(streaminfo.channels) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
		default:
			channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | streaminfo.channels)];
			break;
	}

	_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:streaminfo.sample_freq interleaved:NO channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDMusepack;

	sourceStreamDescription.mSampleRate			= streaminfo.sample_freq;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)streaminfo.channels;

	sourceStreamDescription.mFramesPerPacket	= (1 << streaminfo.block_pwr);

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	_buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:MPC_FRAME_LENGTH];
	_buffer.frameLength = 0;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_demux) {
		mpc_demux_exit(_demux);
		_demux = NULL;
	}

	mpc_reader_exit_stdio(&_reader);

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _demux != NULL;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return _frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	AVAudioFrameCount framesProcessed = 0;

	for(;;) {
		AVAudioFrameCount framesRemaining = frameLength - framesProcessed;
		AVAudioFrameCount framesCopied = [buffer appendContentsOfBuffer:_buffer readOffset:0 frameLength:framesRemaining];
		[_buffer trimAtOffset:0 frameLength:framesCopied];

		framesProcessed += framesCopied;

		// All requested frames were read
		if(framesProcessed == frameLength)
			break;

		// Decode one frame of MPC data
		MPC_SAMPLE_FORMAT buf [MPC_DECODER_BUFFER_LENGTH];
		mpc_frame_info frame;
		frame.buffer = buf;

		if(mpc_demux_decode(_demux, &frame)) {
			os_log_error(gSFBAudioDecoderLog, "Musepack decoding error");
			break;
		}

		// End of input
		if(frame.bits == -1)
			break;

#ifdef MPC_FIXED_POINT
#error "Fixed point not yet supported"
#else
		// Clip the samples to [-1, 1)
		float minValue = -1.f;
		float maxValue = 8388607.f / 8388608.f;

		AVAudioChannelCount channelCount = _buffer.format.channelCount;
		vDSP_vclip((float *)frame.buffer, 1, &minValue, &maxValue, (float *)frame.buffer, 1, frame.samples * channelCount);

		// Deinterleave the normalized samples
		float * const *floatChannelData = _buffer.floatChannelData;
		for(AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
			const float *input = (float *)frame.buffer + channel;
			float *output = floatChannelData[channel] + _buffer.frameLength;
			for(uint32_t sample = 0; sample < frame.samples; ++sample) {
				*output++ = *input;
				input += channelCount;
			}
		}

		_buffer.frameLength = frame.samples;
#endif /* MPC_FIXED_POINT */
	}

	_framePosition += framesProcessed;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	if(mpc_demux_seek_sample(_demux, (mpc_uint64_t)frame))
		return NO;
	_framePosition = frame;
	return YES;
}

@end
