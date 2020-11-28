/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#import <mpg123/mpg123.h>

#import "SFBMPEGDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "NSError+SFBURLPresentation.h"

// ========================================
// Initialization
static void Setupmpg123(void) __attribute__ ((constructor));
static void Setupmpg123()
{
	// What happens if this fails?
	int result = mpg123_init();
	if(result != MPG123_OK)
		os_log_debug(gSFBAudioDecoderLog, "Unable to initialize mpg123: %s", mpg123_plain_strerror(result));
}

static void Teardownmpg123(void) __attribute__ ((destructor));
static void Teardownmpg123()
{
	mpg123_exit();
}

// ========================================
// Callbacks
static ssize_t read_callback(void *iohandle, void *ptr, size_t size)
{
	NSCParameterAssert(iohandle != NULL);

	SFBMPEGDecoder *decoder = (__bridge SFBMPEGDecoder *)iohandle;

	NSInteger bytesRead;
	if(![decoder->_inputSource readBytes:ptr length:(NSInteger)size bytesRead:&bytesRead error:nil])
		return -1;
	return (ssize_t)bytesRead;
}

static off_t lseek_callback(void *iohandle, off_t offset, int whence)
{
	NSCParameterAssert(iohandle != NULL);

	SFBMPEGDecoder *decoder = (__bridge SFBMPEGDecoder *)iohandle;

	if(!decoder->_inputSource.supportsSeeking)
		return -1;

	// Adjust offset as required
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

	if(![decoder->_inputSource seekToOffset:offset error:nil])
		return -1;

	return offset;
}

@interface SFBMPEGDecoder ()
{
@private
	mpg123_handle *_mpg123;
	AVAudioFramePosition _framePosition;
	AVAudioPCMBuffer *_buffer;
}
@end

@implementation SFBMPEGDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"mp3"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/mpeg"];
}

- (BOOL)decodingIsLossless
{
	return NO;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	_mpg123 = mpg123_new(NULL, NULL);

	if(!_mpg123) {
		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MP3 file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid MP3 file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Force decode to floating point instead of 16-bit signed integer
	mpg123_param(_mpg123, MPG123_FLAGS, MPG123_FORCE_FLOAT | MPG123_SKIP_ID3V2 | MPG123_GAPLESS | MPG123_QUIET, 0);
	mpg123_param(_mpg123, MPG123_RESYNC_LIMIT, 2048, 0);

	if(mpg123_replace_reader_handle(_mpg123, read_callback, lseek_callback, NULL) != MPG123_OK) {
		mpg123_delete(_mpg123);
		_mpg123 = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MP3 file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid MP3 file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	if(mpg123_open_handle(_mpg123, (__bridge void *)self) != MPG123_OK) {
		mpg123_delete(_mpg123);
		_mpg123 = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MP3 file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid MP3 file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	long rate;
	int channels, encoding;
	if(mpg123_getformat(_mpg123, &rate, &channels, &encoding) != MPG123_OK || encoding != MPG123_ENC_FLOAT_32 || channels <= 0) {
		mpg123_close(_mpg123);
		mpg123_delete(_mpg123);
		_mpg123 = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MP3 file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid MP3 file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	AVAudioChannelLayout *channelLayout = nil;
	switch(channels) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		default:
			channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:(kAudioChannelLayoutTag_Unknown | (UInt32)channels)];
			break;
	}

	_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:rate interleaved:NO channelLayout:channelLayout];

	size_t bufferSizeBytes = mpg123_outblock(_mpg123);
	UInt32 framesPerMPEGFrame = (UInt32)(bufferSizeBytes / ((size_t)channels * sizeof(float)));

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= kAudioFormatMPEGLayer3;

	struct mpg123_frameinfo mi;
	if(mpg123_info(_mpg123, &mi) == MPG123_OK) {
		switch(mi.layer) {
			case 1: 	sourceStreamDescription.mFormatID = kAudioFormatMPEGLayer1; 	break;
			case 2: 	sourceStreamDescription.mFormatID = kAudioFormatMPEGLayer2; 	break;
			case 3: 	sourceStreamDescription.mFormatID = kAudioFormatMPEGLayer3; 	break;
		}
	}

	sourceStreamDescription.mSampleRate			= rate;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)channels;

	sourceStreamDescription.mFramesPerPacket	= framesPerMPEGFrame;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	if(mpg123_scan(_mpg123) != MPG123_OK) {
		mpg123_close(_mpg123);
		mpg123_delete(_mpg123);
		_mpg123 = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInvalidFormat
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid MP3 file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a valid MP3 file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	_buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:framesPerMPEGFrame];
	_buffer.frameLength = 0;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_mpg123) {
		mpg123_close(_mpg123);
		mpg123_delete(_mpg123);
		_mpg123 = NULL;
	}

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _mpg123 != NULL;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return mpg123_length(_mpg123);
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:paramErr userInfo:nil];
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

		// Read and decode an MPEG frame
		off_t frameNumber;
		unsigned char *audioData = NULL;
		size_t bytesDecoded = 0;
		int result = mpg123_decode_frame(_mpg123, &frameNumber, &audioData, &bytesDecoded);
		// EOS
		if(result == MPG123_DONE)
			break;
		else if(result != MPG123_OK) {
			os_log_error(gSFBAudioDecoderLog, "mpg123_decode_frame failed: %s", mpg123_strerror(_mpg123));
			break;
		}

		// Deinterleave the samples
		AVAudioFrameCount framesDecoded = (AVAudioFrameCount)(bytesDecoded / (sizeof(float) * _buffer.format.channelCount));

		float * const *floatChannelData = _buffer.floatChannelData;
		AVAudioChannelCount channelCount = _buffer.format.channelCount;
		for(AVAudioChannelCount channel = 0; channel < channelCount; ++channel) {
			const float *input = (float *)audioData + channel;
			float *output = floatChannelData[channel];
			for(AVAudioFrameCount frame = 0; frame < framesDecoded; ++frame) {
				*output++ = *input;
				input += channelCount;
			}
		}

		_buffer.frameLength = framesDecoded;
	}

	_framePosition += framesProcessed;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);
	off_t offset = mpg123_seek(_mpg123, frame, SEEK_SET);
	if(offset >= 0)
		_framePosition = offset;
	return offset >= 0;
}

@end
