//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

#include <dumb/dumb.h>

#import "SFBModuleDecoder.h"

#import "NSError+SFBURLPresentation.h"

SFBAudioDecoderName const SFBAudioDecoderNameModule = @"org.sbooth.AudioEngine.Decoder.Module";

#define DUMB_SAMPLE_RATE	65536
#define DUMB_CHANNELS		2
#define DUMB_BIT_DEPTH		16
#define DUMB_BUF_FRAMES		512

static int skip_callback(void *f, dumb_off_t n)
{
	NSCParameterAssert(f != NULL);

	SFBModuleDecoder *decoder = (__bridge SFBModuleDecoder *)f;

	NSInteger offset;
	if(![decoder->_inputSource getOffset:&offset error:nil])
		return 1;

	return ![decoder->_inputSource seekToOffset:(offset + n) error:nil];
}

static int getc_callback(void *f)
{
	NSCParameterAssert(f != NULL);

	SFBModuleDecoder *decoder = (__bridge SFBModuleDecoder *)f;

	uint8_t value;
	if(![decoder->_inputSource readUInt8:&value error:nil])
		return -1;
	return (int)value;
}

static dumb_ssize_t getnc_callback(char *ptr, size_t n, void *f)
{
	NSCParameterAssert(f != NULL);

	SFBModuleDecoder *decoder = (__bridge SFBModuleDecoder *)f;

	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:ptr length:(NSInteger)n bytesRead:&bytesRead error:nil])
		return -1;
	return bytesRead;
}

static void close_callback(void *f)
{
#pragma unused(f)
}

static int seek_callback(void *f, dumb_off_t offset)
{
	NSCParameterAssert(f != NULL);

	SFBModuleDecoder *decoder = (__bridge SFBModuleDecoder *)f;

	if(![decoder->_inputSource seekToOffset:offset error:nil])
		return -1;
	return 0;
}

static dumb_off_t get_size_callback(void *f)
{
	NSCParameterAssert(f != NULL);

	SFBModuleDecoder *decoder = (__bridge SFBModuleDecoder *)f;

	NSInteger length;
	if(![decoder->_inputSource getLength:&length error:nil])
		return -1;
	return length;
}


@interface SFBModuleDecoder ()
{
@private
	DUMBFILE_SYSTEM _dfs;
	DUMBFILE *_df;
	DUH *_duh;
	DUH_SIGRENDERER *_dsr;
	sample_t **_samples;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
}
- (BOOL)openDecoderReturningError:(NSError **)error;
- (void)closeDecoder;
@end

@implementation SFBModuleDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithArray:@[@"it", @"xm", @"s3m", @"stm", @"mod", @"ptm", @"669", @"psm", @"mtm", /*@"riff",*/ @"asy", @"amf", @"okt"]];
}

+ (NSSet *)supportedMIMETypes
{
	// FIXME: Add additional MIME types?
	return [NSSet setWithArray:@[@"audio/it", @"audio/xm", @"audio/s3m", @"audio/mod", @"audio/x-mod"]];
}

+ (SFBAudioDecoderName)decoderName
{
	return SFBAudioDecoderNameModule;
}

- (BOOL)decodingIsLossless
{
	return NO;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	// Generate interleaved 2-channel 16-bit output
	AVAudioChannelLayout *layout = [[AVAudioChannelLayout alloc] initWithLayoutTag:kAudioChannelLayoutTag_Stereo];
	_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:DUMB_SAMPLE_RATE interleaved:YES channelLayout:layout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= kSFBAudioFormatModule;

	sourceStreamDescription.mSampleRate			= DUMB_SAMPLE_RATE;
	sourceStreamDescription.mChannelsPerFrame	= DUMB_CHANNELS;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	return [self openDecoderReturningError:error];
}

- (BOOL)closeReturningError:(NSError **)error
{
	[self closeDecoder];
	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _df != NULL && _duh != NULL && _dsr != NULL;
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
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	if(frameLength == 0)
		return YES;

	AVAudioFrameCount framesProcessed = 0;

	for(;;) {
		AVAudioFrameCount framesRemaining = frameLength - framesProcessed;
		AVAudioFrameCount framesToCopy = MIN(framesRemaining, DUMB_BUF_FRAMES);

		long samplesSize = framesToCopy;
		long framesCopied = duh_render_int(_dsr, &_samples, &samplesSize, DUMB_BIT_DEPTH, 0, 1, 65536.0f / DUMB_SAMPLE_RATE, framesToCopy, buffer.int16ChannelData[0] + (framesProcessed * DUMB_CHANNELS));
		if(framesCopied != framesToCopy)
			os_log_error(gSFBAudioDecoderLog, "duh_render_int() returned short frame count: requested %d, got %ld", framesToCopy, framesCopied);

		framesProcessed += framesCopied;

		// All requested frames were read or EOS reached
		if(framesProcessed == frameLength || framesCopied == 0 || duh_sigrenderer_get_position(_dsr) > _frameLength)
			break;
	}

	_framePosition += framesProcessed;
	buffer.frameLength = (AVAudioFrameCount)framesProcessed;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	// DUMB cannot seek backwards, so the decoder must be reset
	if(frame < _framePosition) {
		[self closeDecoder];
		if(![_inputSource seekToOffset:0 error:error] || ![self openDecoderReturningError:error])
			return NO;
		_framePosition = 0;
	}

	AVAudioFramePosition framesToSkip = frame - _framePosition;
	duh_sigrenderer_generate_samples(_dsr, 1, 65536.0f / DUMB_SAMPLE_RATE, framesToSkip, NULL);
	_framePosition += framesToSkip;

	return YES;
}

- (BOOL)openDecoderReturningError:(NSError **)error
{
	_dfs.open = NULL;
	_dfs.skip = skip_callback;
	_dfs.getc = getc_callback;
	_dfs.getnc = getnc_callback;
	_dfs.close = close_callback;
	_dfs.seek = seek_callback;
	_dfs.get_size = get_size_callback;

	_df = dumbfile_open_ex((__bridge void *)self, &_dfs);
	if(!_df) {
		os_log_error(gSFBAudioDecoderLog, "dumbfile_open_ex failed");
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	_duh = dumb_read_any(_df, 0, 0);
	if(!_duh) {
		dumbfile_close(_df);
		_df = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Module file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a Module file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// NB: This must change if the sample rate changes because it is based on 65536 Hz
	_frameLength = duh_get_length(_duh);

	// Generate 2-channel audio
	_dsr = duh_start_sigrenderer(_duh, 0, DUMB_CHANNELS, 0);
	if(!_dsr) {
		os_log_error(gSFBAudioDecoderLog, "duh_start_sigrenderer failed");

		unload_duh(_duh);
		_duh = NULL;

		dumbfile_close(_df);
		_df = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Module file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a Module file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	_samples = allocate_sample_buffer(DUMB_CHANNELS, DUMB_BUF_FRAMES);
	if(!_samples) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	return YES;
}

- (void)closeDecoder
{
	if(_dsr) {
		duh_end_sigrenderer(_dsr);
		_dsr = NULL;
	}

	if(_duh) {
		unload_duh(_duh);
		_duh = NULL;
	}

	if(_df) {
		dumbfile_close(_df);
		_df = NULL;
	}

	if(_samples) {
		destroy_sample_buffer(_samples);
		_samples = NULL;
	}
}

@end
