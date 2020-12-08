/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <memory>

#include <lame/lame.h>

#import "SFBMP3Encoder.h"

#import "SFBCStringForOSType.h"

SFBAudioEncoderName const SFBAudioEncoderNameMP3 = @"org.sbooth.AudioEngine.Encoder.MP3";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3TargetIsBitrate = @"Encoding Target is Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Quality = @"Quality";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3Bitrate = @"Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3EnableCBR = @"Enable CBR";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3EnableFastVBR = @"Enable Fast VBR Mode";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3VBRQuality = @"VBR Quality";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3StereoMode = @"Stereo Mode";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyMP3CalculateReplayGain = @"Calculate Replay Gain";

SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeMono = @"Mono";
SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeStereo = @"Stereo";
SFBAudioEncodingSettingsValueMP3StereoMode const SFBAudioEncodingSettingsValueMP3StereoModeJointStereo = @"Joint Stereo";

template <>
struct ::std::default_delete<lame_global_flags> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(lame_global_flags *gfp) const noexcept { lame_close(gfp); }
};

@interface SFBMP3Encoder ()
{
@private
	std::unique_ptr<lame_global_flags> _gfp;
	AVAudioFramePosition _framePosition;
}
@end

@implementation SFBMP3Encoder

+ (void)load
{
	[SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"mp3"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/mpeg"];
}

+ (SFBAudioEncoderName)encoderName
{
	return SFBAudioEncoderNameMP3;
}

- (BOOL)encodingIsLossless
{
	return NO;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat
{
	NSParameterAssert(sourceFormat != nil);

	// Validate format
	if(sourceFormat.channelCount < 1 || sourceFormat.channelCount > 2)
		return nil;

	return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:sourceFormat.sampleRate channels:(AVAudioChannelCount)sourceFormat.channelCount interleaved:YES];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	auto gfp = std::unique_ptr<lame_global_flags>(lame_init());
	if(!gfp) {
		os_log_error(gSFBAudioEncoderLog, "lame_init failed");
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	// Write Xing header
	lame_set_bWriteVbrTag(gfp.get(), 1);

	// Initialize the LAME encoder
	auto result = lame_set_num_channels(gfp.get(), (int)_processingFormat.channelCount);
	if(result == -1) {
		os_log_error(gSFBAudioEncoderLog, "lame_set_num_channels(%d) failed", _processingFormat.channelCount);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	result = lame_set_in_samplerate(gfp.get(), (int)_processingFormat.sampleRate);
	if(result == -1) {
		os_log_error(gSFBAudioEncoderLog, "lame_set_in_samplerate(%f) failed", _processingFormat.sampleRate);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	result = lame_init_params(gfp.get());
	if(result == -1) {
		os_log_error(gSFBAudioEncoderLog, "lame_init_params failed");
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	// Adjust encoder settings

	NSNumber *quality = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3Quality];
	if(quality != nil) {
		auto quality_value = quality.intValue;
		switch(quality_value) {
			case 0 ... 9:
				result = lame_set_quality(_gfp.get(), quality_value);
				if(result == -1) {
					os_log_error(gSFBAudioEncoderLog, "lame_set_quality(%d) failed", quality_value);
					if(error)
						*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
					return NO;
				}
				break;
			default:
				os_log_info(gSFBAudioEncoderLog, "Ignoring invalid LAME quality: %d", quality_value);
				break;
		}
	}

	BOOL targetIsBitrate = [[_settings objectForKey:SFBAudioEncodingSettingsKeyMP3TargetIsBitrate] boolValue];
	if(!targetIsBitrate) {
		auto fastVBR = [[_settings objectForKey:SFBAudioEncodingSettingsKeyMP3EnableFastVBR] boolValue];
		result = lame_set_VBR(_gfp.get(), fastVBR ? vbr_mtrh : vbr_rh);
		if(result == -1) {
			os_log_error(gSFBAudioEncoderLog, "lame_set_VBR(%d) failed", fastVBR ? vbr_mtrh : vbr_rh);
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}

		NSNumber *vbrQuality = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3VBRQuality];
		if(vbrQuality != nil) {
			result = lame_set_VBR_quality(_gfp.get(), vbrQuality.floatValue);
			if(result == -1) {
				os_log_error(gSFBAudioEncoderLog, "lame_set_VBR_quality(%f) failed", vbrQuality.floatValue);
				if(error)
					*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
				return NO;
			}
		}
	}
	else {
		auto bitrate = [[_settings objectForKey:SFBAudioEncodingSettingsKeyMP3Bitrate] intValue] * 1000;
		result = lame_set_brate(_gfp.get(), bitrate);
		if(result == -1) {
			os_log_error(gSFBAudioEncoderLog, "lame_set_brate(%d) failed", bitrate);
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}

		auto enableCBR = [[_settings objectForKey:SFBAudioEncodingSettingsKeyMP3EnableCBR] boolValue];
		if(enableCBR) {
			result = lame_set_VBR(_gfp.get(), vbr_off);
			if(result == -1) {
				os_log_error(gSFBAudioEncoderLog, "lame_set_VBR(vbr_off) failed");
				if(error)
					*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
				return NO;
			}
		}
		else {
			result = lame_set_VBR(_gfp.get(), vbr_default);
			if(result == -1) {
				os_log_error(gSFBAudioEncoderLog, "lame_set_VBR(vbr_default) failed");
				if(error)
					*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
				return NO;
			}

			result = lame_set_VBR_min_bitrate_kbps(_gfp.get(), bitrate);
			if(result == -1) {
				os_log_error(gSFBAudioEncoderLog, "lame_set_VBR_min_bitrate_kbps(%d) failed", bitrate);
				if(error)
					*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
				return NO;
			}
		}
	}

	SFBAudioEncodingSettingsValue stereoMode = [_settings objectForKey:SFBAudioEncodingSettingsKeyMP3StereoMode];
	if(stereoMode != nil) {
		if(stereoMode == SFBAudioEncodingSettingsValueMP3StereoModeMono)				result = lame_set_mode(_gfp.get(), MONO);
		else if(stereoMode == SFBAudioEncodingSettingsValueMP3StereoModeStereo)			result = lame_set_mode(_gfp.get(), STEREO);
		else if(stereoMode == SFBAudioEncodingSettingsValueMP3StereoModeJointStereo)	result = lame_set_mode(_gfp.get(), JOINT_STEREO);
		else
			os_log_info(gSFBAudioEncoderLog, "Ignoring unknown LAME stereo mode: %{public}@", stereoMode);

		if(result == -1) {
			os_log_error(gSFBAudioEncoderLog, "lame_set_mode failed");
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}
	}

	auto calculateReplayGain = [[_settings objectForKey:SFBAudioEncodingSettingsKeyMP3CalculateReplayGain] boolValue];
	result = lame_set_findReplayGain(gfp.get(), calculateReplayGain);
	if(result == -1) {
		os_log_error(gSFBAudioEncoderLog, "lame_set_findReplayGain(%d) failed", calculateReplayGain);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	AudioStreamBasicDescription outputStreamDescription{};
	outputStreamDescription.mFormatID			= kAudioFormatMPEGLayer3;
	outputStreamDescription.mSampleRate			= _processingFormat.sampleRate;
	outputStreamDescription.mChannelsPerFrame	= _processingFormat.channelCount;
	_outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

	_gfp = std::move(gfp);

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_gfp.reset();

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _gfp != nullptr;
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

	const size_t bufsize = (size_t)(1.25 * (_processingFormat.channelCount * frameLength)) + 7200;
	auto buf = std::make_unique<unsigned char[]>(bufsize);
	if(!buf) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	auto result = lame_encode_buffer_interleaved_ieee_float(_gfp.get(), (const float *)buffer.audioBufferList->mBuffers[0].mData, (int)frameLength, buf.get(), (int)bufsize);
	if(result == -1) {
		os_log_error(gSFBAudioEncoderLog, "lame_encode_buffer_interleaved_ieee_float failed");
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	NSInteger bytesWritten;
	if(![_outputSource writeBytes:buf.get() length:result bytesWritten:&bytesWritten error:error] || bytesWritten != result)
		return NO;

	_framePosition += frameLength;

	return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error
{
	const size_t bufsize = 7200;
	auto buf = std::make_unique<unsigned char[]>(bufsize);
	if(!buf) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	auto result = lame_encode_flush(_gfp.get(), buf.get(), bufsize);
	if(result == -1) {
		os_log_error(gSFBAudioEncoderLog, "lame_encode_flush failed");
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	NSInteger bytesWritten;
	if(![_outputSource writeBytes:buf.get() length:result bytesWritten:&bytesWritten error:error] || bytesWritten != result)
		return NO;

	return YES;
}

@end
