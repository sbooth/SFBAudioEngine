/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>

#pragma clang diagnostic pop

#import "SFBFLACEncoder.h"

#define DEFAULT_PADDING 8192

@interface SFBFLACEncoder ()
{
@private
	FLAC__StreamEncoder *_flac;
	FLAC__StreamMetadata *_seektable;
	FLAC__StreamMetadata *_padding;
	FLAC__StreamMetadata *_metadata [2];
@package
	AVAudioFramePosition _framePosition;
}
@end

#pragma mark FLAC Callbacks

static FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void *client_data)
{
#pragma unused(encoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
	SFBOutputSource *outputSource = flacEncoder->_outputSource;

	NSInteger bytesWritten;
	if(![outputSource writeBytes:(const void *)buffer length:(NSInteger)bytes bytesWritten:&bytesWritten error:nil] || bytesWritten != (NSInteger)bytes)
		return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

	if(samples > 0)
		flacEncoder->_framePosition = current_frame;

	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderSeekStatus seek_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
#pragma unused(encoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
	SFBOutputSource *outputSource = flacEncoder->_outputSource;

	if(!outputSource.supportsSeeking)
		return FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;

	if(![outputSource seekToOffset:(NSInteger)absolute_byte_offset error:nil])
		return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;

	return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static FLAC__StreamEncoderTellStatus tell_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
#pragma unused(encoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
	SFBOutputSource *outputSource = flacEncoder->_outputSource;

	NSInteger offset;
	if(![outputSource getOffset:&offset error:nil])
		return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;

	*absolute_byte_offset = (FLAC__uint64)offset;

	return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static void metadata_callback(const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
#pragma unused(encoder)
	NSCParameterAssert(client_data != NULL);

	SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
	SFBOutputSource *outputSource = flacEncoder->_outputSource;
}

@implementation SFBFLACEncoder

+ (void)load
{
	[SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"flac"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/flac"];
}

- (BOOL)encodingIsLossless
{
	return YES;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat
{
	NSParameterAssert(sourceFormat != nil);

	// Validate format
	if(sourceFormat.streamDescription->mFormatFlags & kAudioFormatFlagIsFloat || sourceFormat.channelCount < 1 || sourceFormat.channelCount > 8)
		return nil;

	// Set up the processing format
	AudioStreamBasicDescription streamDescription = {0};

	streamDescription.mFormatID				= kAudioFormatLinearPCM;
	streamDescription.mFormatFlags			= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

	streamDescription.mSampleRate			= sourceFormat.sampleRate;
	streamDescription.mChannelsPerFrame		= sourceFormat.channelCount;
	streamDescription.mBitsPerChannel		= ((sourceFormat.streamDescription->mBitsPerChannel + 7) / 8) * 8;
	if(streamDescription.mBitsPerChannel == 32)
		streamDescription.mFormatFlags		|= kAudioFormatFlagIsPacked;

	streamDescription.mBytesPerPacket		= sizeof(FLAC__int32) * streamDescription.mChannelsPerFrame;
	streamDescription.mFramesPerPacket		= 1;
	streamDescription.mBytesPerFrame		= streamDescription.mBytesPerPacket * streamDescription.mFramesPerPacket;

	AVAudioChannelLayout *channelLayout = nil;
	switch(sourceFormat.channelCount) {
		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];				break;
		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];			break;
		case 3:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_3_0_A];		break;
		case 4:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Quadraphonic];		break;
		case 5:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_0_A];		break;
		case 6:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_5_1_A];		break;
		case 7:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_6_1_A];		break;
		case 8:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_MPEG_7_1_A];		break;
	}

	return [[AVAudioFormat alloc] initWithStreamDescription:&streamDescription channelLayout:channelLayout];
}

- (BOOL)openWithSourceFormat:(AVAudioFormat *)sourceFormat error:(NSError **)error
{
	if(![super openWithSourceFormat:sourceFormat error:error])
		return NO;

	// Create FLAC encoder
	_flac = FLAC__stream_encoder_new();
	if(!_flac) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

//	dispatch_block_t  encoderCleanupBlock = ^{
//		if(!FLAC__stream_encoder_finish(self->_flac))
//			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(self->_flac));
//		FLAC__stream_encoder_delete(self->_flac);
//		self->_flac = NULL;
//	};

	// Output format
	if(!FLAC__stream_encoder_set_sample_rate(_flac, (uint32_t)_processingFormat.sampleRate)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_sample_rate failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		if(!FLAC__stream_encoder_finish(_flac))
			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		FLAC__stream_encoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:@{
				NSLocalizedDescriptionKey: NSLocalizedString(@"The output format is not supported by FLAC.", @""),
				NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Unsupported sample rate", @""),
				NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The sample rate is not supported.", @"")
			}];

		return NO;
	}

	if(!FLAC__stream_encoder_set_channels(_flac, _processingFormat.channelCount)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_channels failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		if(!FLAC__stream_encoder_finish(_flac))
			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		FLAC__stream_encoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:@{
				NSLocalizedDescriptionKey: NSLocalizedString(@"The output format is not supported by FLAC.", @""),
				NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Unsupported channel count", @""),
				NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The channel count is not supported.", @"")
			}];

		return NO;
	}

	if(!FLAC__stream_encoder_set_bits_per_sample(_flac, _processingFormat.streamDescription->mBitsPerChannel)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_bits_per_sample failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		if(!FLAC__stream_encoder_finish(_flac))
			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		FLAC__stream_encoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:@{
				NSLocalizedDescriptionKey: NSLocalizedString(@"The output format is not supported by FLAC.", @""),
				NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Unsupported bits per sample", @""),
				NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The bits per sample is not supported.", @"")
			}];

		return NO;
	}

	// Encoder compression level
	NSNumber *compressionLevel = [_settings objectForKey:SFBAudioEncodingSettingsKeyFLACCompressionLevel];
	if(compressionLevel) {
		unsigned int value = compressionLevel.unsignedIntValue;
		switch(value) {
			case 1 ... 8:
				if(!FLAC__stream_encoder_set_compression_level(_flac, value)) {
					os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_compression_level failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

					if(!FLAC__stream_encoder_finish(_flac))
						os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

					FLAC__stream_encoder_delete(_flac);
					_flac = NULL;

					if(error)
						*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

					return NO;
				}
				break;
			default:
				os_log_info(gSFBAudioEncoderLog, "Invalid FLAC compression level: %d", value);
				break;
		}
	}

	NSNumber *verifyEncoding = [_settings objectForKey:SFBAudioEncodingSettingsKeyFLACVerifyEncoding];
	if(verifyEncoding) {
		FLAC__bool value = verifyEncoding.boolValue;
		if(!FLAC__stream_encoder_set_verify(_flac, value)) {
			os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_verify failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

			if(!FLAC__stream_encoder_finish(_flac))
				os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

			FLAC__stream_encoder_delete(_flac);
			_flac = NULL;

			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

			return NO;
		}

	}

	// Create a seektable
	_seektable = FLAC__metadata_object_new(FLAC__METADATA_TYPE_SEEKTABLE);
	if(_seektable == NULL) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_new failed");

		if(!FLAC__stream_encoder_finish(_flac))
			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		FLAC__stream_encoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];

		return NO;
	}

	// Append seekpoints (one every 30 seconds)
//	if(!FLAC__metadata_object_seektable_template_append_spaced_points_by_samples(_seektable, 30 * _processingFormat.sampleRate, totalFrames)) {
//
//	}
//	NSAssert(YES == result, NSLocalizedStringFromTable(@"Unable to allocate memory.", @"Exceptions", @""));

	// Sort the table
	if(!FLAC__metadata_object_seektable_template_sort(_seektable, false)) {

	}

	_metadata[0] = _seektable;

	// Create the padding metadata block
	_padding = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
	if(_padding == NULL) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_new failed");

		if(!FLAC__stream_encoder_finish(_flac))
			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		FLAC__stream_encoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];

		return NO;
	}

	_padding->length = DEFAULT_PADDING;
	_metadata[1] = _padding;

	if(!FLAC__stream_encoder_set_metadata(_flac, _metadata, 2)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_metadata failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		if(!FLAC__stream_encoder_finish(_flac))
			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		FLAC__stream_encoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

		return NO;
	}

	// Initialize the FLAC encoder
//	result = FLAC__stream_encoder_set_total_samples_estimate(_flac, totalFrames);
//	NSAssert1(YES == result, @"FLAC__stream_encoder_set_total_samples_estimate failed: %s", FLAC__stream_encoder_get_resolved_state_string(_flac));

	FLAC__StreamEncoderInitStatus encoderStatus = FLAC__stream_encoder_init_stream(_flac, write_callback, seek_callback, tell_callback, metadata_callback, (__bridge void *)self);
	if(encoderStatus != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_init_stream failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		if(!FLAC__stream_encoder_finish(_flac))
			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		FLAC__stream_encoder_delete(_flac);
		_flac = NULL;

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

		return NO;
	}

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_flac) {
		if(!FLAC__stream_encoder_finish(_flac))
			os_log_info(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));

		FLAC__stream_encoder_delete(_flac);
		_flac = NULL;
	}

	if(_seektable) {
		FLAC__metadata_object_delete(_seektable);
		_seektable = NULL;
	}

	if(_padding) {
		FLAC__metadata_object_delete(_padding);
		_padding = NULL;
	}

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

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(gSFBAudioEncoderLog, "-encodeFromBuffer:frameLength:error: called with invalid parameters");
		return NO;
	}

	if(frameLength > buffer.frameLength)
		frameLength = buffer.frameLength;

	if(!FLAC__stream_encoder_process_interleaved(_flac, (const FLAC__int32 *)buffer.audioBufferList->mBuffers[0].mData, frameLength)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_process_interleaved failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	return YES;
}

@end
