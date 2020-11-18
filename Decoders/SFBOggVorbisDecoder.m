/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#include <vorbis/vorbisfile.h>

#pragma clang diagnostic pop

#import "SFBOggVorbisDecoder.h"

#import "AVAudioChannelLayout+SFBChannelLabels.h"
#import "NSError+SFBURLPresentation.h"

static size_t read_func_callback(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	NSCParameterAssert(datasource != NULL);

	SFBOggVorbisDecoder *decoder = (__bridge SFBOggVorbisDecoder *)datasource;
	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:ptr length:(NSInteger)(size * nmemb) bytesRead:&bytesRead error:nil])
		return 0;
	return (size_t)bytesRead;
}

static int seek_func_callback(void *datasource, ogg_int64_t offset, int whence)
{
	NSCParameterAssert(datasource != NULL);

	SFBOggVorbisDecoder *decoder = (__bridge SFBOggVorbisDecoder *)datasource;

	switch(whence) {
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

static long tell_func_callback(void *datasource)
{
	NSCParameterAssert(datasource != NULL);

	SFBOggVorbisDecoder *decoder = (__bridge SFBOggVorbisDecoder *)datasource;
	NSInteger offset;
	if(![decoder->_inputSource getOffset:&offset error:nil])
		return -1;
	return (long)offset;
}

@interface SFBOggVorbisDecoder ()
{
@private
	OggVorbis_File _vorbisFile;
}
@end

@implementation SFBOggVorbisDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithArray:@[@"ogg", @"oga"]];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/ogg-vorbis"];
}

- (BOOL)decodingIsLossless
{
	return NO;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	ov_callbacks callbacks = {
		.read_func = read_func_callback,
		.seek_func = seek_func_callback,
		.tell_func = tell_func_callback,
		.close_func = NULL
	};

	if(ov_test_callbacks((__bridge void *)self, &_vorbisFile, NULL, 0, callbacks)) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Ogg Vorbis file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not an Ogg Vorbis file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	if(ov_test_open(&_vorbisFile)) {
		os_log_error(gSFBAudioDecoderLog, "ov_test_open failed");

		if(ov_clear(&_vorbisFile))
			os_log_error(gSFBAudioDecoderLog, "ov_clear failed");

		return NO;
	}

	vorbis_info *ovInfo = ov_info(&_vorbisFile, -1);
	if(!ovInfo) {
		os_log_error(gSFBAudioDecoderLog, "ov_info failed");

		if(ov_clear(&_vorbisFile))
			os_log_error(gSFBAudioDecoderLog, "ov_clear failed");

		return NO;
	}

	AVAudioChannelLayout *channelLayout = nil;
	switch(ovInfo->channels) {
			// Default channel layouts from Vorbis I specification section 4.3.9
			// http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 3:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_AC3_3_0];			break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
		case 5:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_0_C];		break;
		case 6:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_1_C];		break;
		case 7:
			channelLayout = [AVAudioChannelLayout layoutWithChannelLabels:7,
							 kAudioChannelLabel_Left, kAudioChannelLabel_Center, kAudioChannelLabel_Right,
							 kAudioChannelLabel_LeftSurround, kAudioChannelLabel_RightSurround, kAudioChannelLabel_CenterSurround,
							 kAudioChannelLabel_LFEScreen];
			break;
		case 8:
			channelLayout = [AVAudioChannelLayout layoutWithChannelLabels:8,
							 kAudioChannelLabel_Left, kAudioChannelLabel_Center, kAudioChannelLabel_Right,
							 kAudioChannelLabel_LeftSurround, kAudioChannelLabel_RightSurround, kAudioChannelLabel_RearSurroundLeft, kAudioChannelLabel_RearSurroundRight,
							 kAudioChannelLabel_LFEScreen];
			break;
		default:
			channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | (UInt32)ovInfo->channels)];
			break;
	}

	_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:ovInfo->rate interleaved:NO channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDVorbis;

	sourceStreamDescription.mSampleRate			= ovInfo->rate;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)ovInfo->channels;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(ov_clear(&_vorbisFile))
		os_log_error(gSFBAudioDecoderLog, "ov_clear failed");

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _vorbisFile.datasource != NULL;
}

- (AVAudioFramePosition)framePosition
{
	ogg_int64_t framePosition = ov_pcm_tell(&_vorbisFile);
	if(framePosition == OV_EINVAL)
		return SFB_UNKNOWN_FRAME_POSITION;
	return framePosition;
}

- (AVAudioFramePosition)frameLength
{
	ogg_int64_t frameLength = ov_pcm_total(&_vorbisFile, -1);
	if(frameLength == OV_EINVAL)
		return SFB_UNKNOWN_FRAME_LENGTH;
	return frameLength;
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

	AVAudioFrameCount framesRemaining = frameLength;
	float **pcm_channels = NULL;
	int bitstream = 0;

	while(framesRemaining > 0) {
		// Decode a chunk of samples from the file
		long framesRead = ov_read_float(&_vorbisFile, &pcm_channels, (int)framesRemaining, &bitstream);

		if(framesRead < 0) {
			os_log_error(gSFBAudioDecoderLog, "Ogg Vorbis decoding error");
			return NO;
		}

		// 0 frames indicates EOS
		if(framesRead == 0)
			break;

		// Copy the frames from the decoding buffer to the output buffer
		float * const *floatChannelData = buffer.floatChannelData;
		AVAudioChannelCount channelCount = buffer.format.channelCount;
		for(AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
			const float *input = pcm_channels[channel];
			float *output = floatChannelData[channel] + buffer.frameLength;
			memcpy(output, input, (size_t)framesRead * sizeof(float));
		}

		buffer.frameLength += (AVAudioFrameCount)framesRead;
		framesRemaining -= framesRead;
	}

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);
	if(ov_pcm_seek(&_vorbisFile, frame)) {
		os_log_error(gSFBAudioDecoderLog, "Ogg Vorbis seek error");
		return NO;
	}
	return YES;
}

@end
