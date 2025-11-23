//
// Copyright (c) 2006-2025 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

// TODO: Figure out a way to selectively disable diagnostic warnings for module imports
@import mpg123;
@import AVFAudioExtensions;

#import "SFBMPEGDecoder.h"

#import "NSData+SFBExtensions.h"
#import "NSError+SFBURLPresentation.h"

SFBAudioDecoderName const SFBAudioDecoderNameMPEG = @"org.sbooth.AudioEngine.Decoder.MPEG";

// ========================================
// Initialization
static void Setupmpg123(void) __attribute__ ((constructor));
static void Setupmpg123(void)
{
	// What happens if this fails?
	int result = mpg123_init();
	if(result != MPG123_OK)
		os_log_fault(gSFBAudioDecoderLog, "Unable to initialize mpg123: %{public}s", mpg123_plain_strerror(result));
}

static void Teardownmpg123(void) __attribute__ ((destructor));
static void Teardownmpg123(void)
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

static BOOL contains_mp3_sync_word_and_minimal_valid_frame_header(uint8_t *buf, NSInteger len)
{
	NSCParameterAssert(buf != NULL);
	NSCParameterAssert(len >= 3);

	uint8_t *loc = buf;
	while(loc) {
		// Search for first byte of MP3 sync word
		loc = memchr(loc, 0xff, len - 3);
		if(loc) {
			// Check whether a complete MP3 sync word was found and perform a minimal check for a valid MP3 frame header
			if((*(loc+1) & 0xe0) == 0xe0 && (*(loc+1) & 0x18) != 0x08 && (*(loc+1) & 0x06) != 0 && (*(loc+2) & 0xf0) != 0xf0 && (*(loc+2) & 0x0c) != 0x0c)
				return YES;
			len -= (loc - buf);
		}
	}

	return NO;
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

+ (SFBAudioDecoderName)decoderName
{
	return SFBAudioDecoderNameMPEG;
}

+ (BOOL)testInputSource:(SFBInputSource *)inputSource formatIsSupported:(SFBTernaryTruthValue *)formatIsSupported error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);
	NSParameterAssert(formatIsSupported != NULL);

	NSInteger originalOffset;
	if(![inputSource getOffset:&originalOffset error:error])
		return NO;

	if(![inputSource seekToOffset:0 error:error])
		return NO;

	NSInteger offset = 0;

	// Attempt to detect and minimally parse an ID3v2 tag header
	NSData *data = [inputSource readDataOfLength:SFBID3v2HeaderSize error:error];
	if([data isID3v2Header])
		offset = [data id3v2TagTotalSize];
	// Skip tag data
	if(![inputSource seekToOffset:offset error:error])
		return NO;

	uint8_t buf [512];
	NSInteger len;
	if(![inputSource readBytes:buf length:sizeof buf bytesRead:&len error:error])
		return NO;

	for(;;) {
		if(len < 2 * SFBMP3DetectionSize) {
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:@{ NSURLErrorKey: inputSource.url }];
			return NO;
		}

		// Search for an MP3 sync word and a frame header that appears to be valid
		if(contains_mp3_sync_word_and_minimal_valid_frame_header(buf, len - 3)) {
			*formatIsSupported = SFBTernaryTruthValueTrue;
			break;
		}

		// Slide last 3 bytes to beginning to restart search
		memmove(buf, buf + len - 3, 3);
		if(![inputSource readBytes:buf + 3 length:sizeof buf - 3 bytesRead:&len error:error])
			return NO;
		len += 3;

		// Limit searches to 2 KB
		NSInteger currentOffset;
		if(![inputSource getOffset:&currentOffset error:error])
			return NO;

		if(currentOffset > offset + 2048) {
			*formatIsSupported = SFBTernaryTruthValueFalse;
			break;
		}
	}

	if(![inputSource seekToOffset:originalOffset error:error])
		return NO;

	return YES;
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

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription channelLayout:channelLayout];

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
		AVAudioFrameCount framesCopied = [buffer appendFromBuffer:_buffer readingFromOffset:0 frameLength:framesRemaining];
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
			os_log_error(gSFBAudioDecoderLog, "mpg123_decode_frame failed: %{public}s", mpg123_strerror(_mpg123));
			if(error)
				*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:@{ NSURLErrorKey: _inputSource.url }];
			return NO;
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
