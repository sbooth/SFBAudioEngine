//
// Copyright (c) 2006 - 2023 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

@import opus.file;
@import AVFAudioExtensions;

#import "SFBOggOpusDecoder.h"

#import "NSError+SFBURLPresentation.h"

SFBAudioDecoderName const SFBAudioDecoderNameOggOpus = @"org.sbooth.AudioEngine.Decoder.OggOpus";

#define OPUS_SAMPLE_RATE 48000

static int read_callback(void *stream, unsigned char *ptr, int nbytes)
{
	NSCParameterAssert(stream != NULL);

	SFBOggOpusDecoder *decoder = (__bridge SFBOggOpusDecoder *)stream;
	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:ptr length:nbytes bytesRead:&bytesRead error:nil])
		return -1;
	return (int)bytesRead;
}

static int seek_callback(void *stream, opus_int64 offset, int whence)
{
	NSCParameterAssert(stream != NULL);

	SFBOggOpusDecoder *decoder = (__bridge SFBOggOpusDecoder *)stream;
	if(!decoder->_inputSource.supportsSeeking)
		return -1;

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

static opus_int64 tell_callback(void *stream)
{
	NSCParameterAssert(stream != NULL);

	SFBOggOpusDecoder *decoder = (__bridge SFBOggOpusDecoder *)stream;
	NSInteger offset;
	if(![decoder->_inputSource getOffset:&offset error:nil])
		return -1;
	return offset;
}

@interface SFBOggOpusDecoder ()
{
@private
	OggOpusFile *_opusFile;
}
@end

@implementation SFBOggOpusDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"opus"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/ogg; codecs=opus"];
}

+ (SFBAudioDecoderName)decoderName
{
	return SFBAudioDecoderNameOggOpus;
}

- (BOOL)decodingIsLossless
{
	return NO;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	OpusFileCallbacks callbacks = {
		.read = read_callback,
		.seek = seek_callback,
		.tell = tell_callback,
		.close = NULL
	};

	_opusFile = op_test_callbacks((__bridge void *)self, &callbacks, NULL, 0, NULL);
	if(!_opusFile) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Ogg Opus file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not an Ogg Opus file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	if(op_test_open(_opusFile)) {
		os_log_error(gSFBAudioDecoderLog, "op_test_open failed");

		op_free(_opusFile);
		_opusFile = NULL;

		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];

		return NO;
	}

	const OpusHead *header = op_head(_opusFile, 0);

	AVAudioChannelLayout *channelLayout = nil;
	switch(header->channel_count) {
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
			channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | (UInt32)header->channel_count)];
			break;
	}

	_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:OPUS_SAMPLE_RATE interleaved:YES channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= kAudioFormatOpus;

	sourceStreamDescription.mSampleRate			= header->input_sample_rate;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)header->channel_count;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_opusFile) {
		op_free(_opusFile);
		_opusFile = NULL;
	}

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _opusFile != NULL;
}

- (AVAudioFramePosition)framePosition
{
	ogg_int64_t framePosition = op_pcm_tell(_opusFile);
	if(framePosition == OP_EINVAL)
		return SFBUnknownFramePosition;
	return framePosition;
}

- (AVAudioFramePosition)frameLength
{
	ogg_int64_t frameLength = op_pcm_total(_opusFile, -1);
	if(frameLength == OP_EINVAL)
		return SFBUnknownFrameLength;
	return frameLength;
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

	AVAudioFrameCount framesRemaining = frameLength;
	while(framesRemaining > 0) {
		// Decode a chunk of samples from the file
		int framesRead = op_read_float(_opusFile, buffer.floatChannelData[0] + (buffer.frameLength * buffer.stride), (int)(framesRemaining * buffer.stride), NULL);

		if(framesRead < 0) {
			os_log_error(gSFBAudioDecoderLog, "Ogg Opus decoding error");
			if(error)
				*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
			return NO;
		}

		// 0 frames indicates EOS
		if(framesRead == 0)
			break;

		buffer.frameLength += (AVAudioFrameCount)framesRead;
		framesRemaining -= (AVAudioFrameCount)framesRead;
	}

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);
	if(op_pcm_seek(_opusFile, frame)) {
		os_log_error(gSFBAudioDecoderLog, "op_pcm_seek() failed");
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
		return NO;
	}
	return YES;
}

@end
