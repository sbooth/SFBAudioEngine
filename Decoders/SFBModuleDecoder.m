/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#include <dumb/dumb.h>

#import "SFBModuleDecoder.h"

#import "NSError+SFBURLPresentation.h"

#define DUMB_SAMPLE_RATE	65536
#define DUMB_CHANNELS		2
#define DUMB_BIT_DEPTH		16

static int skip_callback(void *f, long n)
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

static long getnc_callback(char *ptr, long n, void *f)
{
	NSCParameterAssert(f != NULL);

	SFBModuleDecoder *decoder = (__bridge SFBModuleDecoder *)f;

	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:ptr length:n bytesRead:&bytesRead error:nil])
		return -1;
	return bytesRead;
}

static void close_callback(void *f)
{
#pragma unused(f)
}

@interface SFBModuleDecoder ()
{
@private
	DUMBFILE_SYSTEM _dfs;
	DUMBFILE *_df;
	DUH *_duh;
	DUH_SIGRENDERER *_dsr;
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
	return [NSSet setWithArray:@[@"it", @"xm", @"s3m", @"mod"]];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/it", @"audio/xm", @"audio/s3m", @"audio/mod", @"audio/x-mod"]];
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

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDModule;

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

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	// EOF reached
	if(duh_sigrenderer_get_position(_dsr) > _frameLength)
		return YES;

	long framesRendered = duh_render(_dsr, DUMB_BIT_DEPTH, 0, 1, 65536.0f / DUMB_SAMPLE_RATE, frameLength, buffer.int16ChannelData[0]);

	_framePosition += framesRendered;
	buffer.frameLength = (AVAudioFrameCount)framesRendered;

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

	_df = dumbfile_open_ex((__bridge void *)self, &_dfs);
	if(!_df) {
		os_log_error(gSFBAudioDecoderLog, "dumbfile_open_ex failed");
		return NO;
	}

	NSString *pathExtension = _inputSource.url.pathExtension.lowercaseString;

	// Attempt to create the appropriate decoder based on the file's extension
	if([pathExtension isEqualToString:@"it"])
		_duh = dumb_read_it(_df);
	else if([pathExtension isEqualToString:@"xm"])
		_duh = dumb_read_xm(_df);
	else if([pathExtension isEqualToString:@"s3m"])
		_duh = dumb_read_s3m(_df);
	else if([pathExtension isEqualToString:@"mod"])
		_duh = dumb_read_mod(_df);

	if(!_duh) {
		dumbfile_close(_df);
		_df = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
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
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Module file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a Module file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

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
}

@end
