/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <opus/opusenc.h>

#pragma clang diagnostic pop

#import "SFBOggOpusEncoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"

SFBAudioEncoderName const SFBAudioEncoderNameOggOpus = @"org.sbooth.AudioEngine.Encoder.OggOpus";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusPreserveSampleRate = @"Preserve Sample Rate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusComplexity = @"Complexity";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusBitrate = @"Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusBitrateMode = @"Bitrate Mode";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusSignalType = @"Signal Type";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusFrameDuration = @"Frame Duration";

static int write_callback(void *user_data, const unsigned char *ptr, opus_int32 len)
{
	SFBOggOpusEncoder *encoder = (__bridge SFBOggOpusEncoder *)user_data;
	NSInteger bytesWritten;
	return !([encoder->_outputSource writeBytes:ptr length:len bytesWritten:&bytesWritten error:nil] && bytesWritten == len);
}

static int close_callback(void *user_data)
{
	SFBOggOpusEncoder *encoder = (__bridge SFBOggOpusEncoder *)user_data;
	return ![encoder->_outputSource closeReturningError:nil];
}

@interface SFBOggOpusEncoder ()
{
@private
	OggOpusEnc *_enc;
	OggOpusComments *_comments;
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
	if([[_settings objectForKey:SFBAudioEncodingSettingsKeyOggOpusPreserveSampleRate] boolValue]) {
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

	OggOpusComments *comments = ope_comments_create();
	if(comments == NULL) {
		os_log_error(gSFBAudioEncoderLog, "ope_comments_create failed");
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
		return NO;
	}

	char version [128];
	snprintf(version, 128, "SFBAudioEngine Ogg Opus Encoder (%s)", opus_get_version_string());
	int result = ope_comments_add(comments, "ENCODER", version);
	if(result != OPE_OK) {
		os_log_error(gSFBAudioEncoderLog, "ope_comments_add(ENCODER) failed: %{public}s", ope_strerror(result));

		ope_comments_destroy(comments);

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

		return NO;
	}

	OggOpusEnc *enc = ope_encoder_create_callbacks(&callbacks, (__bridge  void*)self, comments, (opus_int32)_processingFormat.sampleRate, (int)_processingFormat.channelCount, _processingFormat.channelCount > 8 ? 255 : _processingFormat.channelCount > 2, &result);
	if(enc == NULL) {
		os_log_error(gSFBAudioEncoderLog, "ope_encoder_create_callbacks failed: %{public}s", ope_strerror(result));

		ope_comments_destroy(comments);

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

		return NO;
	}

	NSNumber *bitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggOpusBitrate];
	if(bitrate != nil) {
		opus_int32 intValue = bitrate.intValue;
		switch(intValue) {
			case 6 ... 256:
				result = ope_encoder_ctl(enc, OPUS_SET_BITRATE(MIN(256 * (opus_int32)_processingFormat.channelCount, intValue) * 1000));
				if(result != OPE_OK) {
					os_log_error(gSFBAudioEncoderLog, "OPUS_SET_BITRATE failed: %{public}s", ope_strerror(result));

					ope_encoder_destroy(enc);
					ope_comments_destroy(comments);

					if(error)
						*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

					return NO;
				}
				break;
			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Ogg Opus bitrate: %d", intValue);
				break;
		}
	}

	NSNumber *bitrateMode = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggOpusBitrateMode];
	if(bitrateMode != nil) {
		int intValue = bitrateMode.intValue;
		switch(intValue) {
			case SFBAudioEncoderOggOpusBitrateModeVBR:
				result = ope_encoder_ctl(enc, OPUS_SET_VBR(1));
				break;
			case SFBAudioEncoderOggOpusBitrateModeConstrainedVBR:
				result = ope_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(1));
				break;
			case SFBAudioEncoderOggOpusBitrateModeHardCBR:
				result = ope_encoder_ctl(enc, OPUS_SET_VBR(0));
				break;
			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Ogg Opus bitrate mode: %d", intValue);
				break;
		}

		if(result != OPE_OK) {
			os_log_error(gSFBAudioEncoderLog, "OPUS_SET_VBR[_CONSTRAINT] failed: %{public}s", ope_strerror(result));

			ope_encoder_destroy(enc);
			ope_comments_destroy(comments);

			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

			return NO;
		}
	}

	NSNumber *complexity = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggOpusComplexity];
	if(complexity != nil) {
		int intValue = complexity.intValue;
		switch(intValue) {
			case 0 ... 10:
				result = ope_encoder_ctl(enc, OPUS_SET_COMPLEXITY(intValue));
				if(result != OPE_OK) {
					os_log_error(gSFBAudioEncoderLog, "OPUS_SET_COMPLEXITY failed: %{public}s", ope_strerror(result));

					ope_encoder_destroy(enc);
					ope_comments_destroy(comments);

					if(error)
						*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

					return NO;
				}
				break;

			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Ogg Opus complexity: %d", intValue);
				break;
		}
	}

	NSNumber *signalType = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggOpusSignalType];
	if(signalType != nil) {
		int intValue = signalType.intValue;
		switch(intValue) {
			case SFBAudioEncoderOggOpusSignalTypeVoice:
				result = ope_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
				break;
			case SFBAudioEncoderOggOpusSignalTypeMusic:
				result = ope_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
				break;
			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Ogg Opus signal type: %d", intValue);
				break;
		}

		if(result != OPE_OK) {
			os_log_error(gSFBAudioEncoderLog, "OPUS_SET_SIGNAL failed: %{public}s", ope_strerror(result));

			ope_encoder_destroy(enc);
			ope_comments_destroy(comments);

			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

			return NO;
		}
	}

	// Default in opusenc.c
	AVAudioFrameCount frameCapacity = 960;

	NSNumber *frameDuration = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggOpusFrameDuration];
	if(frameDuration != nil) {
		int intValue = frameDuration.intValue;
		switch(intValue) {
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration2_5ms:
				frameCapacity = 120;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_2_5_MS));
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration5ms:
				frameCapacity = 240;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_5_MS));
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration10ms:
				frameCapacity = 480;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_10_MS));
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration20ms:
				frameCapacity = 960;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration40ms:
				frameCapacity = 1920;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_40_MS));
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration60ms:
				frameCapacity = 2880;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_60_MS));
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration80ms:
				frameCapacity = 3840;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_80_MS));
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration100ms:
				frameCapacity = 4800;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_100_MS));
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration120ms:
				frameCapacity = 5760;
				result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_120_MS));
				break;

			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Ogg Opus frame duration: %d", intValue);
				break;
		}

		if(result != OPE_OK) {
			os_log_error(gSFBAudioEncoderLog, "OPUS_SET_EXPERT_FRAME_DURATION failed: %{public}s", ope_strerror(result));

			ope_encoder_destroy(enc);
			ope_comments_destroy(comments);

			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

			return NO;
		}
	}

	_frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:frameCapacity];

	AudioStreamBasicDescription outputStreamDescription = {0};
	outputStreamDescription.mFormatID			= kAudioFormatOpus;
	outputStreamDescription.mSampleRate			= _processingFormat.sampleRate;
	outputStreamDescription.mChannelsPerFrame	= _processingFormat.channelCount;
	_outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

	_enc = enc;
	_comments = comments;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	if(_enc) {
		ope_encoder_destroy(_enc);
		_enc = NULL;
	}

	if(_comments) {
		ope_comments_destroy(_comments);
		_comments = NULL;
	}

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _enc != NULL;
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
		if(error)
			*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:paramErr userInfo:nil];
		return NO;
	}

	if(frameLength > buffer.frameLength)
		frameLength = buffer.frameLength;

	// Split buffer into Opus page-sized chunks
	AVAudioFrameCount framesProcessed = 0;

	for(;;) {
		AVAudioFrameCount framesCopied = [_frameBuffer appendContentsOfBuffer:buffer readOffset:framesProcessed];
		framesProcessed += framesCopied;

		// Encode the next Opus frame
		if(_frameBuffer.isFull) {
			int result = ope_encoder_write_float(_enc, (float *)_frameBuffer.audioBufferList->mBuffers[0].mData, (int)_frameBuffer.frameLength);
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
		int result = ope_encoder_write_float(_enc, (float *)_frameBuffer.audioBufferList->mBuffers[0].mData, (int)_frameBuffer.frameLength);
		if(result != OPE_OK) {
			os_log_error(gSFBAudioEncoderLog, "ope_encoder_write_float failed: %{public}s", ope_strerror(result));
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}

		_framePosition += _frameBuffer.frameLength;
		_frameBuffer.frameLength = 0;
	}

	int result = ope_encoder_drain(_enc);
	if(result != OPE_OK) {
		os_log_error(gSFBAudioEncoderLog, "ope_encoder_drain failed: %{public}s", ope_strerror(result));
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	return YES;
}

@end
