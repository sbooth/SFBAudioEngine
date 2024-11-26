//
// Copyright (c) 2020-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <memory>

#import <os/log.h>

#import <FLAC/metadata.h>
#import <FLAC/stream_encoder.h>

#import "SFBFLACEncoder.h"

#define DEFAULT_PADDING 8192

SFBAudioEncoderName const SFBAudioEncoderNameFLAC = @"org.sbooth.AudioEngine.Encoder.FLAC";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACCompressionLevel = @"Compression Level";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyFLACVerifyEncoding = @"Verify Encoding";

namespace {

/// A deleter class for FLAC__StreamEncoder objects
struct stream_encoder_deleter {
	void operator()(FLAC__StreamEncoder *encoder) { FLAC__stream_encoder_delete(encoder); }
};

/// A deleter class for FLAC__StreamMetadata objects
struct stream_metadata_deleter {
	void operator()(FLAC__StreamMetadata *metadata) { FLAC__metadata_object_delete(metadata); }
};

using stream_encoder_unique_ptr = std::unique_ptr<FLAC__StreamEncoder, stream_encoder_deleter>;
using stream_metadata_unique_ptr = std::unique_ptr<FLAC__StreamMetadata, stream_metadata_deleter>;

} /* namespace */

@interface SFBFLACEncoder ()
{
@private
	stream_encoder_unique_ptr _flac;
	stream_metadata_unique_ptr _seektable;
	stream_metadata_unique_ptr _padding;
	FLAC__StreamMetadata *_metadata [2];
@package
	AVAudioFramePosition _framePosition;
}
@end

#pragma mark FLAC Callbacks

namespace {

FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void *client_data)
{
#pragma unused(encoder)
	NSCParameterAssert(client_data != nullptr);

	SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
	SFBOutputSource *outputSource = flacEncoder->_outputSource;

	NSInteger bytesWritten;
	if(![outputSource writeBytes:static_cast<const void *>(buffer) length:static_cast<NSInteger>(bytes) bytesWritten:&bytesWritten error:nil] || bytesWritten != static_cast<NSInteger>(bytes))
		return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

	if(samples > 0)
		flacEncoder->_framePosition = current_frame;

	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC__StreamEncoderSeekStatus seek_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
#pragma unused(encoder)
	NSCParameterAssert(client_data != nullptr);

	SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
	SFBOutputSource *outputSource = flacEncoder->_outputSource;

	if(!outputSource.supportsSeeking)
		return FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;

	if(![outputSource seekToOffset:static_cast<NSInteger>(absolute_byte_offset) error:nil])
		return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;

	return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

FLAC__StreamEncoderTellStatus tell_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
#pragma unused(encoder)
	NSCParameterAssert(client_data != nullptr);

	SFBFLACEncoder *flacEncoder = (__bridge SFBFLACEncoder *)client_data;
	SFBOutputSource *outputSource = flacEncoder->_outputSource;

	NSInteger offset;
	if(![outputSource getOffset:&offset error:nil])
		return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;

	*absolute_byte_offset = static_cast<FLAC__uint64>(offset);

	return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

void metadata_callback(const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
#pragma unused(encoder)
#pragma unused(metadata)
#pragma unused(client_data)
}

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

+ (SFBAudioEncoderName)encoderName
{
	return SFBAudioEncoderNameFLAC;
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
	AudioStreamBasicDescription streamDescription{};

	streamDescription.mFormatID				= kAudioFormatLinearPCM;
	streamDescription.mFormatFlags			= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

	streamDescription.mSampleRate			= sourceFormat.sampleRate;
	streamDescription.mChannelsPerFrame		= sourceFormat.channelCount;
	streamDescription.mBitsPerChannel		= ((sourceFormat.streamDescription->mBitsPerChannel + 7) / 8) * 8;
	if(streamDescription.mBitsPerChannel == 32)
		streamDescription.mFormatFlags		|= kAudioFormatFlagIsPacked;

	streamDescription.mBytesPerPacket		= sizeof(FLAC__int32) * streamDescription.mChannelsPerFrame;
	streamDescription.mFramesPerPacket		= 1;
	streamDescription.mBytesPerFrame		= streamDescription.mBytesPerPacket / streamDescription.mFramesPerPacket;

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

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	// Create FLAC encoder
	stream_encoder_unique_ptr flac{FLAC__stream_encoder_new()};
	if(!flac) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	// Output format
	if(!FLAC__stream_encoder_set_sample_rate(flac.get(), static_cast<uint32_t>(_processingFormat.sampleRate))) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_sample_rate failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(flac.get()));

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:@{
				NSLocalizedDescriptionKey: NSLocalizedString(@"The output format is not supported by FLAC.", @""),
				NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Unsupported sample rate", @""),
				NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The sample rate is not supported.", @"")
			}];

		return NO;
	}

	if(!FLAC__stream_encoder_set_channels(flac.get(), _processingFormat.channelCount)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_channels failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(flac.get()));

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:@{
				NSLocalizedDescriptionKey: NSLocalizedString(@"The output format is not supported by FLAC.", @""),
				NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Unsupported channel count", @""),
				NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The channel count is not supported.", @"")
			}];

		return NO;
	}

	if(!FLAC__stream_encoder_set_bits_per_sample(flac.get(), _processingFormat.streamDescription->mBitsPerChannel)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_bits_per_sample failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(flac.get()));

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:@{
				NSLocalizedDescriptionKey: NSLocalizedString(@"The output format is not supported by FLAC.", @""),
				NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Unsupported bits per sample", @""),
				NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"The bits per sample is not supported.", @"")
			}];

		return NO;
	}

	if(_estimatedFramesToEncode > 0 && !FLAC__stream_encoder_set_total_samples_estimate(flac.get(), static_cast<FLAC__uint64>(_estimatedFramesToEncode))) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_total_samples_estimate failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(flac.get()));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	// Encoder compression level
	NSNumber *compressionLevel = [_settings objectForKey:SFBAudioEncodingSettingsKeyFLACCompressionLevel];
	if(compressionLevel != nil) {
		unsigned int value = compressionLevel.unsignedIntValue;
		switch(value) {
			case 1 ... 8:
				if(!FLAC__stream_encoder_set_compression_level(flac.get(), value)) {
					os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_compression_level failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(flac.get()));
					if(error)
						*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
					return NO;
				}
				break;
			default:
				os_log_info(gSFBAudioEncoderLog, "Ignoring invalid FLAC compression level: %d", value);
				break;
		}
	}

	NSNumber *verifyEncoding = [_settings objectForKey:SFBAudioEncodingSettingsKeyFLACVerifyEncoding];
	if(verifyEncoding != nil) {
		FLAC__bool value = verifyEncoding.boolValue;
		if(!FLAC__stream_encoder_set_verify(flac.get(), value)) {
			os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_verify failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(flac.get()));
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}

	}

	// Create the padding metadata block
	stream_metadata_unique_ptr padding{FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)};
	if(!padding) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_new failed");
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	padding->length = DEFAULT_PADDING;

	// Create a seektable when possible
	stream_metadata_unique_ptr seektable;
	if(_estimatedFramesToEncode > 0) {
		seektable = stream_metadata_unique_ptr{FLAC__metadata_object_new(FLAC__METADATA_TYPE_SEEKTABLE)};
		if(!seektable) {
			os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_new failed");
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
			return NO;
		}

		// Append seekpoints (one every 10 seconds)
		if(!FLAC__metadata_object_seektable_template_append_spaced_points_by_samples(seektable.get(), static_cast<uint32_t>(10 * _processingFormat.sampleRate), static_cast<FLAC__uint64>(_estimatedFramesToEncode))) {
			os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_seektable_template_append_spaced_points_by_samples failed");
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
			return NO;
		}

		// Sort the table
		if(!FLAC__metadata_object_seektable_template_sort(seektable.get(), false)) {
			os_log_error(gSFBAudioEncoderLog, "FLAC__metadata_object_seektable_template_sort failed");
			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
			return NO;
		}
	}

	_metadata[0] = padding.get();
	if(seektable)
		_metadata[1] = seektable.get();

	if(!FLAC__stream_encoder_set_metadata(flac.get(), _metadata, seektable ? 2 : 1)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_set_metadata failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(flac.get()));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	// Initialize the FLAC encoder
	FLAC__StreamEncoderInitStatus encoderStatus = FLAC__stream_encoder_init_stream(flac.get(), write_callback, seek_callback, tell_callback, metadata_callback, (__bridge void *)self);
	if(encoderStatus != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_init_stream failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(flac.get()));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	AudioStreamBasicDescription outputStreamDescription{};
	outputStreamDescription.mFormatID			= kAudioFormatFLAC;
	outputStreamDescription.mSampleRate			= _processingFormat.sampleRate;
	outputStreamDescription.mChannelsPerFrame	= _processingFormat.channelCount;
	outputStreamDescription.mBitsPerChannel		= _processingFormat.streamDescription->mBitsPerChannel;
	switch(outputStreamDescription.mBitsPerChannel) {
		case 16:
			outputStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_16BitSourceData;
			break;
		case 20:
			outputStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_20BitSourceData;
			break;
		case 24:
			outputStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_24BitSourceData;
			break;
		case 32:
			outputStreamDescription.mFormatFlags = kAppleLosslessFormatFlag_32BitSourceData;
			break;
	}
	outputStreamDescription.mFramesPerPacket	= FLAC__stream_encoder_get_blocksize(flac.get());
	_outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

	_flac = std::move(flac);
	_seektable = std::move(seektable);
	_padding = std::move(padding);

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_flac.reset();
	_seektable.reset();
	_padding.reset();

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _flac != nullptr;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (BOOL)encodeFromBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer.format isEqual:_processingFormat]);

	if(frameLength > buffer.frameLength)
		frameLength = buffer.frameLength;

	if(frameLength == 0)
		return YES;

	if(!FLAC__stream_encoder_process_interleaved(_flac.get(), static_cast<const FLAC__int32 *>(buffer.audioBufferList->mBuffers[0].mData), frameLength)) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_process_interleaved failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac.get()));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error
{
	if(!FLAC__stream_encoder_finish(_flac.get())) {
		os_log_error(gSFBAudioEncoderLog, "FLAC__stream_encoder_finish failed: %{public}s", FLAC__stream_encoder_get_resolved_state_string(_flac.get()));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}
	return YES;
}

@end
