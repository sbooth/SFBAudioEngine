/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#pragma clang diagnostic push
//#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <opus/opusenc.h>

#pragma clang diagnostic pop

#import "SFBOggOpusEncoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "SFBCStringForOSType.h"

SFBAudioEncoderName const SFBAudioEncoderNameOggOpus 					= @"org.sbooth.AudioEngine.Encoder.OggOpus";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusPreserveSampleRate		= @"Preserve Sample Rate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusSignalType				= @"Signal Type";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggOpusFrameDuration			= @"Frame Duration";

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

	int family = _processingFormat.channelCount > 8 ? 255 : _processingFormat.channelCount > 2;
	OggOpusEnc *enc = ope_encoder_create_callbacks(&callbacks, (__bridge  void*)self, comments, (opus_int32)_processingFormat.sampleRate, (int)_processingFormat.channelCount, family, &result);
	if(enc == NULL) {
		os_log_error(gSFBAudioEncoderLog, "ope_encoder_create_callbacks failed: %{public}s", ope_strerror(result));

		ope_comments_destroy(comments);

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

		return NO;
	}

	int signal_param = OPUS_AUTO;

	NSNumber *signalType = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggOpusSignalType];
	if(signalType != nil) {
		int intValue = signalType.intValue;
		switch(intValue) {
			case SFBAudioEncoderOggOpusSignalTypeAutomatic:		signal_param = OPUS_AUTO;				break;
			case SFBAudioEncoderOggOpusSignalTypeSpeech:		signal_param = OPUS_SIGNAL_VOICE;		break;
			case SFBAudioEncoderOggOpusSignalTypeMusic:			signal_param = OPUS_SIGNAL_MUSIC;		break;

			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Ogg Opus signal type: %d", intValue);
				break;
		}
	}

	result = ope_encoder_ctl(enc, OPUS_SET_SIGNAL(signal_param));
	if(result != OPE_OK) {
		os_log_error(gSFBAudioEncoderLog, "OPUS_SET_SIGNAL failed: %{public}s", ope_strerror(result));

		ope_encoder_destroy(enc);
		ope_comments_destroy(comments);

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

		return NO;
	}

	AVAudioFrameCount frameSize = 960;
	int frame_param = OPUS_FRAMESIZE_20_MS;

	NSNumber *frameDuration = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggOpusFrameDuration];
	if(frameDuration != nil) {
		int intValue = frameDuration.intValue;
		switch(intValue) {
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration2_5ms:
				frameSize = 120;
				frame_param = OPUS_FRAMESIZE_2_5_MS;
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration5ms:
				frameSize = 240;
				frame_param = OPUS_FRAMESIZE_5_MS;
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration10ms:
				frameSize = 480;
				frame_param = OPUS_FRAMESIZE_10_MS;
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration20ms:
				frameSize = 960;
				frame_param = OPUS_FRAMESIZE_20_MS;
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration40ms:
				frameSize = 1920;
				frame_param = OPUS_FRAMESIZE_40_MS;
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration60ms:
				frameSize = 2880;
				frame_param = OPUS_FRAMESIZE_60_MS;
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration80ms:
				frameSize = 3840;
				frame_param = OPUS_FRAMESIZE_80_MS;
				break;
			case SFBAudioEncodingSettingsKeyOggOpusFrameDuration120ms:
				frameSize = 4800;
				frame_param = OPUS_FRAMESIZE_120_MS;
				break;

			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Ogg Opus frame duration: %d", intValue);
				break;
		}
	}

	result = ope_encoder_ctl(_enc, OPUS_SET_EXPERT_FRAME_DURATION(frame_param));
	if(result != OPE_OK) {
		os_log_error(gSFBAudioEncoderLog, "OPUS_SET_EXPERT_FRAME_DURATION failed: %{public}s", ope_strerror(result));

		ope_encoder_destroy(enc);
		ope_comments_destroy(comments);

		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];

		return NO;
	}

	_frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:frameSize];

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
