/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#include <wavpack/wavpack.h>

#import "SFBWavPackDecoder.h"

#import "NSError+SFBURLPresentation.h"

#define BUFFER_SIZE_FRAMES 2048

static inline AVAudioFrameCount SFB_min(AVAudioFrameCount a, AVAudioFrameCount b) { return a < b ? a : b; }

static int32_t read_bytes_callback(void *id, void *data, int32_t bcount)
{
	NSCParameterAssert(id != NULL);

	SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:data length:bcount bytesRead:&bytesRead error:nil])
		return -1;
	return (int32_t)bytesRead;
}

static int64_t get_pos_callback(void *id)
{
	NSCParameterAssert(id != NULL);

	SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

	NSInteger offset;
	if(![decoder->_inputSource getOffset:&offset error:nil])
		return 0;
	return offset;
}

static int set_pos_abs_callback(void *id, int64_t pos)
{
	NSCParameterAssert(id != NULL);

	SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;
	return ![decoder->_inputSource seekToOffset:pos error:nil];
}

static int set_pos_rel_callback(void *id, int64_t delta, int mode)
{
	NSCParameterAssert(id != NULL);

	SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

	if(!decoder->_inputSource.supportsSeeking)
		return -1;

	// Adjust offset as required
	NSInteger offset = delta;
	switch(mode) {
		case SEEK_SET:
			// offset remains unchanged
			break;
		case SEEK_CUR: {
			NSInteger inputSourceOffset;
			if([decoder->_inputSource getOffset:&inputSourceOffset error:nil])
				offset += inputSourceOffset;
			break;
		}
		case SEEK_END: {
			NSInteger inputSourceLength;
			if([decoder->_inputSource getLength:&inputSourceLength error:nil])
				offset += inputSourceLength;
			break;
		}
	}

	return ![decoder->_inputSource seekToOffset:offset error:nil];
}

// FIXME: How does one emulate ungetc when the data is non-seekable?
// A small read buffer in SFBWavPackDecoder would work but this function
// only seems to be called once per file (when opening) so it may not be worthwhile
static int push_back_byte_callback(void *id, int c)
{
	NSCParameterAssert(id != NULL);

	SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

	if(!decoder->_inputSource.supportsSeeking)
		return EOF;

	NSInteger offset;
	if(![decoder->_inputSource getOffset:&offset error:nil] || offset < 1)
		return EOF;

	if(![decoder->_inputSource seekToOffset:(offset - 1) error:nil])
		return EOF;

	return c;
}

static int64_t get_length_callback(void *id)
{
	NSCParameterAssert(id != NULL);

	SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;

	NSInteger length;
	if(![decoder->_inputSource getLength:&length error:nil])
		return -1;
	return length;
}

static int can_seek_callback(void *id)
{
	NSCParameterAssert(id != NULL);

	SFBWavPackDecoder *decoder = (__bridge SFBWavPackDecoder *)id;
	return (int)decoder->_inputSource.supportsSeeking;
}

@interface SFBWavPackDecoder ()
{
@private
	WavpackStreamReader64 _streamReader;
	WavpackContext *_wpc;
	int32_t *_buffer;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
}
@end

@implementation SFBWavPackDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"wv"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/wavpack", @"audio/x-wavpack"]];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	_streamReader.read_bytes = read_bytes_callback;
	_streamReader.get_pos = get_pos_callback;
	_streamReader.set_pos_abs = set_pos_abs_callback;
	_streamReader.set_pos_rel = set_pos_rel_callback;
	_streamReader.push_back_byte = push_back_byte_callback;
	_streamReader.get_length = get_length_callback;
	_streamReader.can_seek = can_seek_callback;

	char errorBuf [80];

	// Setup converter
	_wpc = WavpackOpenFileInputEx64(&_streamReader, (__bridge void *)self, NULL, errorBuf, OPEN_WVC | OPEN_NORMALIZE/* | OPEN_DSD_NATIVE*/, 0);
	if(!_wpc) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid WavPack file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a WavPack file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	AVAudioChannelLayout *channelLayout = nil;
	switch(WavpackGetNumChannels(_wpc)) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
	}

	// Floating-point and lossy files will be handed off in the canonical Core Audio format
	int mode = WavpackGetMode(_wpc);
	//	int qmode = WavpackGetQualifyMode(_wpc);
	if(MODE_FLOAT & mode || !(MODE_LOSSLESS & mode)) {
		_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:WavpackGetSampleRate(_wpc) interleaved:NO channelLayout:channelLayout];
//		if(channelLayout)
//			_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:WavpackGetSampleRate(_wpc) interleaved:NO channelLayout:channelLayout];
//		else
//			_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:WavpackGetSampleRate(_wpc) channels:WavpackGetNumChannels(_wpc) interleaved:NO];
	}
	//	else if(qmode & QMODE_DSD_AUDIO) {
	//	}
	else {
		AudioStreamBasicDescription processingStreamDescription = {0};

		processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
		processingStreamDescription.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;

		// Align high because Apple's AudioConverter doesn't handle low alignment
		if(WavpackGetBitsPerSample(_wpc) != 32)
			processingStreamDescription.mFormatFlags 	|= kAudioFormatFlagIsAlignedHigh;

		processingStreamDescription.mSampleRate			= WavpackGetSampleRate(_wpc);
		processingStreamDescription.mChannelsPerFrame	= (UInt32)WavpackGetNumChannels(_wpc);
		processingStreamDescription.mBitsPerChannel		= (UInt32)WavpackGetBitsPerSample(_wpc);

		processingStreamDescription.mBytesPerPacket		= 4;
		processingStreamDescription.mFramesPerPacket	= 1;
		processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket * processingStreamDescription.mFramesPerPacket;

		_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];
	}

	_frameLength = WavpackGetNumSamples64(_wpc);

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDWavPack;

	sourceStreamDescription.mSampleRate			= WavpackGetSampleRate(_wpc);
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)WavpackGetNumChannels(_wpc);
	sourceStreamDescription.mBitsPerChannel		= (UInt32)WavpackGetBitsPerSample(_wpc);
	sourceStreamDescription.mBytesPerPacket		= (UInt32)WavpackGetBytesPerSample(_wpc);

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	_buffer = malloc(sizeof(int32_t) * (size_t)BUFFER_SIZE_FRAMES * (size_t)WavpackGetNumChannels(_wpc));
	if(!_buffer) {
		WavpackCloseFile(_wpc);
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];

		return NO;
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_buffer) {
		free(_buffer);
		_buffer = NULL;
	}

	if(_wpc) {
		WavpackCloseFile(_wpc);
		_wpc = NULL;
	}

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _wpc != NULL;
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
		os_log_debug(OS_LOG_DEFAULT, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	AVAudioFrameCount framesRemaining = frameLength;
	while(framesRemaining > 0) {
		uint32_t framesToRead = SFB_min(framesRemaining, BUFFER_SIZE_FRAMES);

		// Wavpack uses "complete" samples (one sample across all channels), i.e. a Core Audio frame
		uint32_t samplesRead = WavpackUnpackSamples(_wpc, _buffer, framesToRead);

		if(samplesRead == 0)
			break;

		// The samples returned are handled differently based on the file's mode
		int mode = WavpackGetMode(_wpc);
//		int qmode = WavpackGetQualifyMode(_wpc);

		// Floating point files require no special handling other than deinterleaving
		if(mode & MODE_FLOAT) {
			float * const *floatChannelData = buffer.floatChannelData;
			AVAudioChannelCount channelCount = buffer.format.channelCount;
			for(AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
				const float *input = (float *)_buffer + channel;
				float *output = floatChannelData[channel] + buffer.frameLength;
				for(uint32_t sample = channel; sample < samplesRead; ++sample) {
					*output++ = *input;
					input += channelCount;
				}
			}

			buffer.frameLength += samplesRead;
		}
		// Lossless files will be handed off as integers
		else if(mode & MODE_LOSSLESS) {
			// WavPack hands us 32-bit signed ints with the samples low-aligned
			int shift = 8 * (4 - WavpackGetBytesPerSample(_wpc));

			int32_t * const *int32ChannelData = buffer.int32ChannelData;
			AVAudioChannelCount channelCount = buffer.format.channelCount;

			// Deinterleave the 32-bit samples, shifting to high alignment
			if(shift) {
				int32_t mask = (1 << (32 - shift)) - 1;
				for(AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
					const int32_t *input = _buffer + channel;
					int32_t *output = int32ChannelData[channel] + buffer.frameLength;
					for(uint32_t sample = channel; sample < samplesRead; ++sample) {
						*output++ = (*input & mask) << shift;
						input += channelCount;
					}
				}
			}
			// Just deinterleave the 32-bit samples
			else {
				for(AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
					const int32_t *input = _buffer + channel;
					int32_t *output = int32ChannelData[channel] + buffer.frameLength;
					for(uint32_t sample = channel; sample < samplesRead; ++sample) {
						*output++ = *input;
						input += channelCount;
					}
				}
			}

			buffer.frameLength += samplesRead;
		}
		// Convert lossy files to float
		else {
			float scaleFactor = (1 << ((WavpackGetBytesPerSample(_wpc) * 8) - 1));

			// Deinterleave the 32-bit samples and convert to float
			float * const *floatChannelData = buffer.floatChannelData;
			AVAudioChannelCount channelCount = buffer.format.channelCount;
			for(AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
				const int32_t *input = _buffer + channel;
				float *output = floatChannelData[channel] + buffer.frameLength;
				for(uint32_t sample = channel; sample < samplesRead; ++sample) {
					*output++ = *input / scaleFactor;
					input += channelCount;
				}
			}

			buffer.frameLength += samplesRead;
		}

		framesRemaining -= samplesRead;
		_framePosition += samplesRead;
	}

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	if(!WavpackSeekSample64(_wpc, frame))
		return NO;

	_framePosition = frame;
	return YES;
}

@end
