/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <memory>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <opus/opusenc.h>

#pragma clang diagnostic pop

#import "SFBOggOpusEncoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"

SFBAudioEncoderName const SFBAudioEncoderNameOggOpus = @"org.sbooth.AudioEngine.Encoder.OggOpus";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusPreserveSampleRate = @"Preserve Sample Rate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusComplexity = @"Complexity";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusBitrate = @"Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusBitrateMode = @"Bitrate Mode";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusSignalType = @"Signal Type";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOpusFrameDuration = @"Frame Duration";

SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeVBR = @"VBR";
SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeConstrainedVBR = @"Constrained VBR";
SFBAudioEncodingSettingsValueOpusBitrateMode const SFBAudioEncodingSettingsValueOpusBitrateModeHardCBR = @"Hard CBR";

SFBAudioEncodingSettingsValueOpusSignalType const SFBAudioEncodingSettingsValueOpusSignalTypeVoice = @"Voice";
SFBAudioEncodingSettingsValueOpusSignalType const SFBAudioEncodingSettingsValueOpusSignalTypeMusic = @"Music";

SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration2_5ms = @"2.5 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration5ms = @"5 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration10ms = @"10 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration20ms = @"20 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration40ms = @"40 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration60ms = @"60 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration80ms = @"80 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration100ms = @"100 msec";
SFBAudioEncodingSettingsValueOpusFrameDuration const SFBAudioEncodingSettingsValueOpusFrameDuration120ms = @"120 msec";

template <>
struct ::std::default_delete<OggOpusComments> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(OggOpusComments *comments) const noexcept { ope_comments_destroy(comments); }
};

template <>
struct ::std::default_delete<OggOpusEnc> {
	default_delete() = default;
	template <class U>
	constexpr default_delete(default_delete<U>) noexcept {}
	void operator()(OggOpusEnc *enc) const noexcept { ope_encoder_destroy(enc); }
};

namespace {
	int write_callback(void *user_data, const unsigned char *ptr, opus_int32 len)
	{
		SFBOggOpusEncoder *encoder = (__bridge SFBOggOpusEncoder *)user_data;
		NSInteger bytesWritten;
		return !([encoder->_outputSource writeBytes:ptr length:len bytesWritten:&bytesWritten error:nil] && bytesWritten == len);
	}

	int close_callback(void *user_data)
	{
		SFBOggOpusEncoder *encoder = (__bridge SFBOggOpusEncoder *)user_data;
		return ![encoder->_outputSource closeReturningError:nil];
	}
}

@interface SFBOggOpusEncoder ()
{
@private
	std::unique_ptr<OggOpusEnc> _enc;
	std::unique_ptr<OggOpusComments> _comments;
	AVAudioPCMBuffer *_frameBuffer;
	AVAudioFramePosition _framePosition;
}
@end

@implementation SFBOggOpusEncoder

+ (void)load
{
	[SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"opus"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/ogg; codecs=opus"];
}

+ (SFBAudioEncoderName)encoderName
{
	return SFBAudioEncoderNameOggOpus;
}

- (BOOL)encodingIsLossless
{
	return NO;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat
{
	NSParameterAssert(sourceFormat != nil);

	// Validate format
	if(sourceFormat.channelCount < 1 || sourceFormat.channelCount > 255)
		return nil;

	double sampleRate = 48000;
	if([[_settings objectForKey:SFBAudioEncodingSettingsKeyOpusPreserveSampleRate] boolValue]) {
		if(sourceFormat.sampleRate < 100 || sourceFormat.sampleRate > 768000)
			return nil;
		sampleRate = sourceFormat.sampleRate;
	}

	return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:sampleRate channels:(AVAudioChannelCount)sourceFormat.channelCount interleaved:YES];
}

- (BOOL)openReturningError:(NSError **)error
{
	//	NSAssert(_processingFormat.sampleRate <= 768000, @"Invalid sample rate: %f", _processingFormat.sampleRate);
	//	NSAssert(_processingFormat.sampleRate >= 100, @"Invalid sample rate: %f", _processingFormat.sampleRate);
	//	NSAssert(_processingFormat.channelCount < 1, @"Invalid channel count: %d", _processingFormat.channelCount);
	//	NSAssert(_processingFormat.channelCount > 255, @"Invalid channel count: %d", _processingFormat.channelCount);

	if(![super openReturningError:error])
		return NO;

	OpusEncCallbacks callbacks = { write_callback, close_callback };

	auto comments = std::unique_ptr<OggOpusComments>(ope_comments_create());
	if(!comments) {
		os_log_error(gSFBAudioEncoderLog, "ope_comments_create failed");
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	char version [128];
	snprintf(version, 128, "SFBAudioEngine Ogg Opus Encoder (%s)", opus_get_version_string());
	int result = ope_comments_add(comments.get(), "ENCODER", version);
	if(result != OPE_OK) {
		os_log_error(gSFBAudioEncoderLog, "ope_comments_add(ENCODER) failed: %{public}s", ope_strerror(result));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	auto enc = std::unique_ptr<OggOpusEnc>((ope_encoder_create_callbacks(&callbacks, (__bridge  void*)self, comments.get(), (opus_int32)_processingFormat.sampleRate, (int)_processingFormat.channelCount, _processingFormat.channelCount > 8 ? 255 : _processingFormat.channelCount > 2, &result)));
	if(!enc) {
		os_log_error(gSFBAudioEncoderLog, "ope_encoder_create_callbacks failed: %{public}s", ope_strerror(result));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	NSNumber *bitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusBitrate];
	if(bitrate != nil) {
		opus_int32 intValue = bitrate.intValue;
		switch(intValue) {
			case 6 ... 256:
				result = ope_encoder_ctl(enc.get(), OPUS_SET_BITRATE(MIN(256 * (opus_int32)_processingFormat.channelCount, intValue) * 1000));
				if(result != OPE_OK) {
					os_log_error(gSFBAudioEncoderLog, "OPUS_SET_BITRATE failed: %{public}s", ope_strerror(result));
					if(error)
						*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
					return NO;
				}
				break;
			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Opus bitrate: %d", intValue);
				break;
		}
	}

	SFBAudioEncodingSettingsValue bitrateMode = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusBitrateMode];
	if(bitrateMode != nil) {
		if(bitrateMode == SFBAudioEncodingSettingsValueOpusBitrateModeVBR)						result = ope_encoder_ctl(enc.get(), OPUS_SET_VBR(1));
		else if(bitrateMode == SFBAudioEncodingSettingsValueOpusBitrateModeConstrainedVBR)		result = ope_encoder_ctl(enc.get(), OPUS_SET_VBR_CONSTRAINT(1));
		else if(bitrateMode == SFBAudioEncodingSettingsValueOpusBitrateModeHardCBR)				result = ope_encoder_ctl(enc.get(), OPUS_SET_VBR(0));
		else
			os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Opus bitrate mode: %{public}@", bitrateMode);

		if(result != OPE_OK) {
			os_log_error(gSFBAudioEncoderLog, "OPUS_SET_VBR[_CONSTRAINT] failed: %{public}s", ope_strerror(result));
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}
	}

	NSNumber *complexity = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusComplexity];
	if(complexity != nil) {
		int intValue = complexity.intValue;
		switch(intValue) {
			case 0 ... 10:
				result = ope_encoder_ctl(enc.get(), OPUS_SET_COMPLEXITY(intValue));
				if(result != OPE_OK) {
					os_log_error(gSFBAudioEncoderLog, "OPUS_SET_COMPLEXITY failed: %{public}s", ope_strerror(result));
					if(error)
						*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
					return NO;
				}
				break;

			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Opus complexity: %d", intValue);
				break;
		}
	}

	SFBAudioEncodingSettingsValue signalType = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusSignalType];
	if(signalType != nil) {
		if(signalType == SFBAudioEncodingSettingsValueOpusSignalTypeVoice)			result = ope_encoder_ctl(enc.get(), OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
		else if(signalType == SFBAudioEncodingSettingsValueOpusSignalTypeMusic)		result = ope_encoder_ctl(enc.get(), OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
		else
			os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Opus signal type: %{public}@", signalType);

		if(result != OPE_OK) {
			os_log_error(gSFBAudioEncoderLog, "OPUS_SET_SIGNAL failed: %{public}s", ope_strerror(result));
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}
	}

	// Default in opusenc.c
	AVAudioFrameCount frameCapacity = 960;

	SFBAudioEncodingSettingsValue frameDuration = [_settings objectForKey:SFBAudioEncodingSettingsKeyOpusFrameDuration];
	if(frameDuration != nil) {
		if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration2_5ms) {
			frameCapacity = 120;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_2_5_MS));
		}
		else if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration5ms) {
			frameCapacity = 240;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_5_MS));
		}
		else if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration10ms) {
			frameCapacity = 480;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_10_MS));
		}
		else if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration20ms) {
			frameCapacity = 960;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));
		}
		else if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration40ms) {
			frameCapacity = 1920;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_40_MS));
		}
		else if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration60ms) {
			frameCapacity = 2880;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_60_MS));
		}
		else if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration80ms) {
			frameCapacity = 3840;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_80_MS));
		}
		else if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration100ms) {
			frameCapacity = 4800;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_100_MS));
		}
		else if(signalType == SFBAudioEncodingSettingsValueOpusFrameDuration120ms) {
			frameCapacity = 5760;
			result = ope_encoder_ctl(enc.get(), OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_120_MS));
		}
		else
			os_log_error(gSFBAudioEncoderLog, "Ignoring unknown Opus frame duration: %{public}@", frameDuration);

		if(result != OPE_OK) {
			os_log_error(gSFBAudioEncoderLog, "OPUS_SET_EXPERT_FRAME_DURATION failed: %{public}s", ope_strerror(result));
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}
	}

	_frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:frameCapacity];

	AudioStreamBasicDescription outputStreamDescription{};
	outputStreamDescription.mFormatID			= kAudioFormatOpus;
	outputStreamDescription.mSampleRate			= _processingFormat.sampleRate;
	outputStreamDescription.mChannelsPerFrame	= _processingFormat.channelCount;
	_outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

	_enc = std::move(enc);
	_comments = std::move(comments);

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_enc.reset();
	_comments.reset();

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _enc != nullptr;
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

	// Split buffer into Opus page-sized chunks
	AVAudioFrameCount framesProcessed = 0;

	for(;;) {
		AVAudioFrameCount framesCopied = [_frameBuffer appendFromBuffer:buffer readingFromOffset:framesProcessed];
		framesProcessed += framesCopied;

		// Encode the next Opus frame
		if(_frameBuffer.isFull) {
			int result = ope_encoder_write_float(_enc.get(), (float *)_frameBuffer.audioBufferList->mBuffers[0].mData, (int)_frameBuffer.frameLength);
			if(result != OPE_OK) {
				os_log_error(gSFBAudioEncoderLog, "ope_encoder_write_float failed: %{public}s", ope_strerror(result));
				if(error)
					*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
				return NO;
			}

			_framePosition += _frameBuffer.frameLength;
			_frameBuffer.frameLength = 0;
		}

		// All complete frames were processed
		if(framesProcessed == frameLength)
			break;
	}

	return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error
{
	// Write remaining partial frame
	if(!_frameBuffer.isEmpty) {
		int result = ope_encoder_write_float(_enc.get(), (float *)_frameBuffer.audioBufferList->mBuffers[0].mData, (int)_frameBuffer.frameLength);
		if(result != OPE_OK) {
			os_log_error(gSFBAudioEncoderLog, "ope_encoder_write_float failed: %{public}s", ope_strerror(result));
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}

		_framePosition += _frameBuffer.frameLength;
		_frameBuffer.frameLength = 0;
	}

	int result = ope_encoder_drain(_enc.get());
	if(result != OPE_OK) {
		os_log_error(gSFBAudioEncoderLog, "ope_encoder_drain failed: %{public}s", ope_strerror(result));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	return YES;
}

@end
