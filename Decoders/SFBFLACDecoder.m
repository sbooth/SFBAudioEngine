/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import OSLog;

#import <FLAC/metadata.h>
#import <FLAC/stream_decoder.h>

#import "SFBFLACDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "NSError+SFBURLPresentation.h"

@interface SFBFLACDecoder ()
{
@private
	FLAC__StreamDecoder *_flac;
	FLAC__StreamMetadata_StreamInfo _streamInfo;
	AVAudioFramePosition _framePosition;
	AVAudioPCMBuffer *_frameBuffer; // For converting push to pull
}
- (FLAC__StreamDecoderWriteStatus)handleFLACWrite:(const FLAC__StreamDecoder *)decoder frame:(const FLAC__Frame *)frame buffer:(const FLAC__int32 * const [])buffer;
- (void)handleFLACMetadata:(const FLAC__StreamDecoder *)decoder metadata:(const FLAC__StreamMetadata *)metadata;
- (void)handleFLACError:(const FLAC__StreamDecoder *)decoder status:(FLAC__StreamDecoderErrorStatus)status;
@end

#pragma mark FLAC Callbacks

static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
#pragma unused(decoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
	SFBInputSource *inputSource = flacDecoder->_inputSource;

	NSInteger bytesRead;
	if(![inputSource readBytes:buffer length:(NSInteger)*bytes bytesRead:&bytesRead error:nil])
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

	*bytes = (size_t)bytesRead;

	if(bytesRead == 0 && inputSource.atEOF)
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
#pragma unused(decoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
	SFBInputSource *inputSource = flacDecoder->_inputSource;

	if(!inputSource.supportsSeeking)
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;

	if(![inputSource seekToOffset:(NSInteger)absolute_byte_offset error:nil])
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;

	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
#pragma unused(decoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;

	NSInteger offset;
	if(![flacDecoder->_inputSource getOffset:&offset error:nil])
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;

	*absolute_byte_offset = (FLAC__uint64)offset;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
#pragma unused(decoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;

	NSInteger length;
	if(![flacDecoder->_inputSource getLength:&length error:nil])
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

	*stream_length = (FLAC__uint64)length;
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
#pragma unused(decoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
	return flacDecoder->_inputSource.atEOF;
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
#pragma unused(decoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
	return [flacDecoder handleFLACWrite:decoder frame:frame buffer:buffer];
}

static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	NSCParameterAssert(client_data != NULL);

	SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
	[flacDecoder handleFLACMetadata:decoder metadata:metadata];
}

static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	NSCParameterAssert(client_data != NULL);

	SFBFLACDecoder *flacDecoder = (__bridge SFBFLACDecoder *)client_data;
	[flacDecoder handleFLACError:decoder status:status];
}

@implementation SFBFLACDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithArray:@[@"flac", @"oga"]];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/flac", @"audio/ogg"]];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	// Create FLAC decoder
	_flac = FLAC__stream_decoder_new();
	if(!_flac) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	// Initialize decoder
	FLAC__StreamDecoderInitStatus status = FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE;

	// Attempt to create a stream decoder based on the file's extension
	NSString *extension = _inputSource.url.pathExtension.lowercaseString;
	if([extension isEqualToString:@"flac"])
		status = FLAC__stream_decoder_init_stream(_flac, read_callback, seek_callback, tell_callback, length_callback, eof_callback, write_callback, metadata_callback, error_callback, (__bridge void *)self);
	else if([extension isEqualToString:@"oga"])
		status = FLAC__stream_decoder_init_ogg_stream(_flac, read_callback, seek_callback, tell_callback, length_callback, eof_callback, write_callback, metadata_callback, error_callback, (__bridge void *)self);

	if(status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		os_log_error(gSFBAudioDecoderLog, "FLAC__stream_decoder_init_xxx failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(_flac));

		if(!FLAC__stream_decoder_finish(_flac))
			os_log_info(gSFBAudioDecoderLog, "FLAC__stream_decoder_finish failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(_flac));

		FLAC__stream_decoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid FLAC file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a FLAC file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Process metadata
	if(!FLAC__stream_decoder_process_until_end_of_metadata(_flac)) {
		os_log_error(gSFBAudioDecoderLog, "FLAC__stream_decoder_process_until_end_of_metadata failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(_flac));

		if(!FLAC__stream_decoder_finish(_flac))
			os_log_info(gSFBAudioDecoderLog, "FLAC__stream_decoder_finish failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(_flac));

		FLAC__stream_decoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid FLAC file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not a FLAC file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Set up the processing format
	AudioStreamBasicDescription processingStreamDescription = {0};

	processingStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	processingStreamDescription.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved;

	processingStreamDescription.mSampleRate			= _streamInfo.sample_rate;
	processingStreamDescription.mChannelsPerFrame	= _streamInfo.channels;
	processingStreamDescription.mBitsPerChannel		= _streamInfo.bits_per_sample;

	processingStreamDescription.mBytesPerPacket		= (_streamInfo.bits_per_sample + 7) / 8;
	processingStreamDescription.mFramesPerPacket	= 1;
	processingStreamDescription.mBytesPerFrame		= processingStreamDescription.mBytesPerPacket * processingStreamDescription.mFramesPerPacket;

	// FLAC supports from 4 to 32 bits per sample
	switch(processingStreamDescription.mBitsPerChannel) {
		case 8:
		case 16:
		case 24:
		case 32:
			processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsPacked;
			break;

		case 4 ... 7:
		case 9 ... 15:
		case 17 ... 23:
		case 25 ... 31:
			// Align high because Apple's AudioConverter doesn't handle low alignment
			processingStreamDescription.mFormatFlags |= kAudioFormatFlagIsAlignedHigh;
			break;

		default: {
			os_log_error(gSFBAudioDecoderLog, "Unsupported bit depth: %u", _streamInfo.bits_per_sample);

			if(!FLAC__stream_decoder_finish(_flac))
				os_log_info(gSFBAudioDecoderLog, "FLAC__stream_decoder_finish failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(_flac));

			FLAC__stream_decoder_delete(_flac);
			_flac = NULL;

			if(error)
				*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
												 code:SFBAudioDecoderErrorCodeInputOutput
						descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a supported FLAC file.", @"")
												  url:_inputSource.url
										failureReason:NSLocalizedString(@"Bit depth not supported", @"")
								   recoverySuggestion:NSLocalizedString(@"The file's bit depth is not supported.", @"")];

			return NO;
		}
	}

	AVAudioChannelLayout *channelLayout = nil;
	switch(_streamInfo.channels) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 3:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_3_0_A];		break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
		case 5:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_0_A];		break;
		case 6:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_1_A];		break;
		case 7:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_6_1_A];		break;
		case 8:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_7_1_A];		break;
	}

	_processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&processingStreamDescription channelLayout:channelLayout];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= kAudioFormatFLAC;

	sourceStreamDescription.mSampleRate			= _streamInfo.sample_rate;
	sourceStreamDescription.mChannelsPerFrame	= _streamInfo.channels;
	sourceStreamDescription.mBitsPerChannel		= _streamInfo.bits_per_sample;

	sourceStreamDescription.mFramesPerPacket	= _streamInfo.max_blocksize;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	// Allocate the buffer list (which will convert from FLAC's push model to Core Audio's pull model)
	_frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:_streamInfo.max_blocksize];
	_frameBuffer.frameLength = 0;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_flac) {
		if(!FLAC__stream_decoder_finish(_flac))
			os_log_info(gSFBAudioDecoderLog, "FLAC__stream_decoder_finish failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(_flac));

		FLAC__stream_decoder_delete(_flac);
		_flac = NULL;
	}

	_frameBuffer = nil;
	memset(&_streamInfo, 0, sizeof(_streamInfo));

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _flac != NULL;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return (AVAudioFramePosition)_streamInfo.total_samples;
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
		AVAudioFrameCount framesCopied = [buffer appendContentsOfBuffer:_frameBuffer readOffset:0 frameLength:framesRemaining];
		[_frameBuffer trimAtOffset:0 frameLength:framesCopied];

		framesProcessed += framesCopied;

		// All requested frames were read or EOS reached
		if(framesProcessed == frameLength || FLAC__stream_decoder_get_state(_flac) == FLAC__STREAM_DECODER_END_OF_STREAM)
			break;

		// Grab the next frame
		if(!FLAC__stream_decoder_process_single(_flac))
			os_log_error(gSFBAudioDecoderLog, "FLAC__stream_decoder_process_single failed: %{public}s", FLAC__stream_decoder_get_resolved_state_string(_flac));
	}

	_framePosition += framesProcessed;

	return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);
//	NSParameterAssert(frame <= _totalFrames);

	FLAC__bool result = FLAC__stream_decoder_seek_absolute(_flac, (FLAC__uint64)frame);

	// Attempt to re-sync the stream if necessary
	if(FLAC__stream_decoder_get_state(_flac) == FLAC__STREAM_DECODER_SEEK_ERROR)
		result = FLAC__stream_decoder_flush(_flac);

	if(result) {
		_framePosition = frame;
		_frameBuffer.frameLength = 0;
	}

	return result != 0;
}

- (FLAC__StreamDecoderWriteStatus)handleFLACWrite:(const FLAC__StreamDecoder *)decoder frame:(const FLAC__Frame *)frame buffer:(const FLAC__int32 * const [])buffer
{
	NSParameterAssert(decoder != NULL);
	NSParameterAssert(frame != NULL);
//	NSAssert(_frameBuffer.frameLength == 0, @"_frameBuffer not empty");
	
	const AudioBufferList *abl = _frameBuffer.audioBufferList;
	if(abl->mNumberBuffers != frame->header.channels)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	const AudioStreamBasicDescription *format = _frameBuffer.format.streamDescription;

	// FLAC hands us 32-bit signed ints with the samples low-aligned; shift them to high alignment
	uint32_t shift = (format->mFormatFlags & kAudioFormatFlagIsPacked) ? 0 : (8 * format->mBytesPerFrame) - format->mBitsPerChannel;

	// Convert to native endian samples, high-aligned if necessary
	switch(format->mBytesPerFrame) {
		case 1: {
			if(shift) {
				for(uint32_t channel = 0; channel < frame->header.channels; ++channel) {
					char *dst = (char *)abl->mBuffers[channel].mData;
					for(uint32_t sample = 0; sample < frame->header.blocksize; ++sample)
						*dst++ = (char)(buffer[channel][sample] << shift);
				}
			}
			else {
				for(uint32_t channel = 0; channel < frame->header.channels; ++channel) {
					char *dst = (char *)abl->mBuffers[channel].mData;
					for(uint32_t sample = 0; sample < frame->header.blocksize; ++sample)
						*dst++ = (char)buffer[channel][sample];
				}
			}
			
			_frameBuffer.frameLength = frame->header.blocksize;
			break;
		}

		case 2: {
			if(shift) {
				for(uint32_t channel = 0; channel < frame->header.channels; ++channel) {
					short *dst = (short *)abl->mBuffers[channel].mData;
					for(uint32_t sample = 0; sample < frame->header.blocksize; ++sample)
						*dst++ = (short)(buffer[channel][sample] << shift);
				}
			}
			else {
				for(uint32_t channel = 0; channel < frame->header.channels; ++channel) {
					short *dst = (short *)abl->mBuffers[channel].mData;
					for(uint32_t sample = 0; sample < frame->header.blocksize; ++sample)
						*dst++ = (short)buffer[channel][sample];
				}
			}

			_frameBuffer.frameLength = frame->header.blocksize;
			break;
		}

		case 3: {
			if(shift) {
				for(uint32_t channel = 0; channel < frame->header.channels; ++channel) {
					unsigned char *dst = (unsigned char *)abl->mBuffers[channel].mData;
					FLAC__int32 value;
					for(uint32_t sample = 0; sample < frame->header.blocksize; ++sample) {
						value = buffer[channel][sample] << shift;
#if __BIG_ENDIAN__
						*dst++ = (unsigned char)((value >> 16) & 0xff);
						*dst++ = (unsigned char)((value >> 8) & 0xff);
						*dst++ = (unsigned char)(value & 0xff);
#elif __LITTLE_ENDIAN__
						*dst++ = (unsigned char)(value & 0xff);
						*dst++ = (unsigned char)((value >> 8) & 0xff);
						*dst++ = (unsigned char)((value >> 16) & 0xff);
#else
#  error Unknown OS byte order
#endif
					}
				}
			}
			else {
				for(uint32_t channel = 0; channel < frame->header.channels; ++channel) {
					unsigned char *dst = (unsigned char *)abl->mBuffers[channel].mData;
					FLAC__int32 value;
					for(uint32_t sample = 0; sample < frame->header.blocksize; ++sample) {
						value = buffer[channel][sample];
#if __BIG_ENDIAN__
						*dst++ = (unsigned char)((value >> 16) & 0xff);
						*dst++ = (unsigned char)((value >> 8) & 0xff);
						*dst++ = (unsigned char)(value & 0xff);
#elif __LITTLE_ENDIAN__
						*dst++ = (unsigned char)(value & 0xff);
						*dst++ = (unsigned char)((value >> 8) & 0xff);
						*dst++ = (unsigned char)((value >> 16) & 0xff);
#else
#  error Unknown OS byte order
#endif
					}
				}
			}

			_frameBuffer.frameLength = frame->header.blocksize;
			break;
		}

		case 4: {
			if(shift) {
				for(uint32_t channel = 0; channel < frame->header.channels; ++channel) {
					int *dst = (int *)abl->mBuffers[channel].mData;
					for(uint32_t sample = 0; sample < frame->header.blocksize; ++sample)
						*dst++ = (int)(buffer[channel][sample] << shift);
				}
			}
			else {
				for(uint32_t channel = 0; channel < frame->header.channels; ++channel) {
					int *dst = (int *)abl->mBuffers[channel].mData;
					for(uint32_t sample = 0; sample < frame->header.blocksize; ++sample)
						*dst++ = (int)buffer[channel][sample];
				}
			}

			_frameBuffer.frameLength = frame->header.blocksize;
			break;
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

- (void)handleFLACMetadata:(const FLAC__StreamDecoder *)decoder metadata:(const FLAC__StreamMetadata *)metadata
{
	NSParameterAssert(metadata != NULL);

	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
		memcpy(&_streamInfo, &metadata->data.stream_info, sizeof(metadata->data.stream_info));
}

- (void)handleFLACError:(const FLAC__StreamDecoder *)decoder status:(FLAC__StreamDecoderErrorStatus)status
{
	os_log_error(gSFBAudioDecoderLog, "FLAC error: %{public}s", FLAC__StreamDecoderErrorStatusString[status]);
}

@end
