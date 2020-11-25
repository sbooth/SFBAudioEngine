/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

@import os.log;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"

#import <speex/speex.h>
#import <speex/speex_header.h>
#import <speex/speex_stereo.h>
#import <speex/speex_callbacks.h>
#import <speex/speex_preprocess.h>

#pragma clang diagnostic pop

#include <ogg/ogg.h>

#import "SFBOggSpeexEncoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "SFBCStringForOSType.h"

SFBAudioEncoderName const SFBAudioEncoderNameOggSpeex 					= @"org.sbooth.AudioEngine.Encoder.OggSpeex";

SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexMode					= @"Encoding Mode";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexTargetIsBitrate		= @"Encoding Target is Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexQuality				= @"Quality";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexComplexity				= @"Complexity";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexBitrate				= @"Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableVBR				= @"Enable VBR";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexVBRMaxBitrate			= @"VBR Maximum Bitrate";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableVAD				= @"Enable VAD";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableDTX				= @"Enable DTX";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableABR				= @"Enable ABR";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexDenoiseInput			= @"Denoise Input";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexEnableAGC				= @"Enable AGC";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexDisableHighpassFilter	= @"Disable Highpass Filter";
SFBAudioEncodingSettingsKey const SFBAudioEncodingSettingsKeyOggSpeexSpeexFramesPerOggPacket= @"Speex Frames per Ogg Packet";

static void vorbis_comment_init(char **comments, size_t *length, const char *vendor_string)
{
	size_t vendor_length = strlen(vendor_string);
	size_t len = 4 + vendor_length + 4;
	char *p = (char *)malloc(len);
	if(p == NULL) {
		*comments = NULL;
		*length = 0;
		return;
	}

	uint32_t user_comment_list_length = 0;

	OSWriteLittleInt32(p, 0, vendor_length);
	memcpy(p + 4, vendor_string, vendor_length);
	OSWriteLittleInt32(p, 4 + vendor_length, user_comment_list_length);

	*length = len;
	*comments = p;
}

#if 0
static void vorbis_comment_add(char **comments, size_t *length, const char *tag, const char *val)
{
	char *p = *comments;

	size_t vendor_length = OSReadLittleInt32(p, 0);
	size_t user_comment_list_length = OSReadLittleInt32(p, 4 + vendor_length);

	size_t tag_len = (tag ? strlen(tag) : 0);
	size_t val_len = strlen(val);
	size_t len = (*length) + 4 + tag_len + val_len;
	p = (char *)reallocf(p, len);
	if(p == NULL) {
		*comments = NULL;
		*length = 0;
		return;
	}

	OSWriteLittleInt32(p, *length, tag_len + val_len);  /* length of comment */
	if(tag) memcpy(p + *length + 4, tag, tag_len);  /* comment */
	memcpy(p + *length + 4 + tag_len, val, val_len);  /* comment */
	OSWriteLittleInt32(p, 4 + vendor_length, user_comment_list_length + 1);

	*comments = p;
	*length = len;
}
#endif

#define MAX_FRAME_BYTES 2000

@interface SFBOggSpeexEncoder ()
{
@private
	ogg_stream_state _os;
	void *_st;
	SpeexPreprocessState *_preprocess;
	SpeexBits _bits;
	AVAudioPCMBuffer *_frameBuffer;
	AVAudioFramePosition _framePosition;
	spx_int32_t _speex_frame_size;
	spx_int32_t _speex_lookahead;
	spx_int32_t _speex_frames_per_ogg_packet;
	ogg_int64_t _speex_frame_number;
}
- (BOOL)encodeSpeexFrameReturningError:(NSError **)error;
@end

@implementation SFBOggSpeexEncoder

+ (void)load
{
	[SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"spx"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/ogg; codecs=speex"];
}

+ (SFBAudioEncoderName)encoderName
{
	return SFBAudioEncoderNameOggSpeex;
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

	double sampleRate = sourceFormat.sampleRate;

	NSNumber *mode = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexMode];
	if(mode != nil) {
		// Determine the desired sample rate
		switch(mode.intValue) {
			case SFBAudioEncoderOggSpeexModeNarrowband:		sampleRate = 8000;		break;
			case SFBAudioEncoderOggSpeexModeWideband:		sampleRate = 16000;		break;
			case SFBAudioEncoderOggSpeexModeUltraWideband:	sampleRate = 32000;		break;
			default:
				return nil;
		}
	}
	else if(sampleRate > 48000 || sampleRate < 6000)
		return nil;

	return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:sampleRate channels:(AVAudioChannelCount)sourceFormat.channelCount interleaved:YES];
}

- (BOOL)openReturningError:(NSError **)error
{
//	NSAssert(_processingFormat.sampleRate <= 48000, @"Invalid sample rate: %f", _processingFormat.sampleRate);
//	NSAssert(_processingFormat.sampleRate >= 6000, @"Invalid sample rate: %f", _processingFormat.sampleRate);
//	NSAssert(_processingFormat.channelCount < 1, @"Invalid channel count: %d", _processingFormat.channelCount);
//	NSAssert(_processingFormat.channelCount > 2, @"Invalid channel count: %d", _processingFormat.channelCount);

	if(![super openReturningError:error])
		return NO;

	// Initialize the ogg stream
	int result = ogg_stream_init(&_os, (int)arc4random());
	if(result == -1) {
		os_log_error(gSFBAudioEncoderLog, "ogg_stream_init failed");
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	// Setup the encoder
	const SpeexMode *speex_mode = NULL;
	NSNumber *mode = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexMode];
	if(mode == nil) {
		if(_processingFormat.sampleRate > 25000)
			speex_mode = speex_lib_get_mode(SPEEX_MODEID_UWB);
		else if(_processingFormat.sampleRate > 12500)
			speex_mode = speex_lib_get_mode(SPEEX_MODEID_WB);
		else if(_processingFormat.sampleRate >= 6000)
			speex_mode = speex_lib_get_mode(SPEEX_MODEID_NB);
	}
	else {
		switch(mode.intValue) {
			case SFBAudioEncoderOggSpeexModeNarrowband:		speex_mode = speex_lib_get_mode(SPEEX_MODEID_NB);		break;
			case SFBAudioEncoderOggSpeexModeWideband:		speex_mode = speex_lib_get_mode(SPEEX_MODEID_WB);		break;
			case SFBAudioEncoderOggSpeexModeUltraWideband:	speex_mode = speex_lib_get_mode(SPEEX_MODEID_UWB);		break;

			default:
				os_log_error(gSFBAudioEncoderLog, "Ignoring invalid Ogg Speex mode: %d", mode.intValue);
				ogg_stream_clear(&_os);
				if(error)
					*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
				return NO;
		}
	}

	// Setup the encoder
	_st = speex_encoder_init(speex_mode);
	if(_st == NULL) {
		os_log_error(gSFBAudioEncoderLog, "Unrecognized Ogg Speex mode: %d", mode.intValue);
		ogg_stream_clear(&_os);
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	speex_encoder_ctl(_st, SPEEX_GET_FRAME_SIZE, &_speex_frame_size);

	_frameBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:(AVAudioFrameCount)_speex_frame_size];

	NSNumber *complexity = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexComplexity] ?: @3;
	spx_int32_t complexity_value = complexity.intValue;
	speex_encoder_ctl(_st, SPEEX_SET_COMPLEXITY, &complexity_value);

	spx_int32_t rate = (spx_int32_t)_processingFormat.sampleRate; // 8, 16, 32
	speex_encoder_ctl(_st, SPEEX_SET_SAMPLING_RATE, &rate);

	spx_int32_t vbr_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexEnableVBR] boolValue];
	spx_int32_t vad_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexEnableVAD] boolValue];
	spx_int32_t dtx_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexEnableDTX] boolValue];
	spx_int32_t abr_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexEnableABR] boolValue];

	NSNumber *quality = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexQuality] ?: @-1;

	// Encoder mode
	if([[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexTargetIsBitrate] boolValue]) {
		NSNumber *bitrate = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexBitrate];
		if(bitrate != nil) {
			spx_int32_t bitrate_value = bitrate.intValue;
			speex_encoder_ctl(_st, SPEEX_SET_BITRATE, &bitrate_value);
		}
		else
			os_log_info(gSFBAudioEncoderLog, "Ogg Speex encoding target is bitrate but no bitrate specified");
	}
	else if(quality.intValue >= 0) {
		spx_int32_t vbr_max = [[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexVBRMaxBitrate] intValue];
		if(vbr_enabled) {
			if(vbr_max > 0)
				speex_encoder_ctl(_st, SPEEX_SET_VBR_MAX_BITRATE, &vbr_max);
			float vbr_quality = quality.floatValue;
			speex_encoder_ctl(_st, SPEEX_SET_VBR_QUALITY, &vbr_quality);
		}
		else {
			spx_int32_t quality_value = quality.intValue;
			speex_encoder_ctl(_st, SPEEX_SET_QUALITY, &quality_value);
		}
	}

	if(vbr_enabled)
		speex_encoder_ctl(_st, SPEEX_SET_VBR, &vbr_enabled);
	else if(vad_enabled)
		speex_encoder_ctl(_st, SPEEX_SET_VAD, &vad_enabled);

	if(dtx_enabled)
		speex_encoder_ctl(_st, SPEEX_SET_DTX, &dtx_enabled);

	if(abr_enabled)
		speex_encoder_ctl(_st, SPEEX_SET_ABR, &abr_enabled);

	if(dtx_enabled && !(vbr_enabled || abr_enabled || vad_enabled))
		os_log_info(gSFBAudioEncoderLog, "DTX requires VAD, VBR, or ABR");
	else if((vbr_enabled || abr_enabled) && (vad_enabled))
		os_log_info(gSFBAudioEncoderLog, "VAD is implied by VBR or ABR");


	spx_int32_t highpass_enabled = ![[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexDisableHighpassFilter] boolValue];
	speex_encoder_ctl(_st, SPEEX_SET_HIGHPASS, &highpass_enabled);

	speex_encoder_ctl(_st, SPEEX_GET_LOOKAHEAD, &_speex_lookahead);

	spx_int32_t denoise_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexDenoiseInput] boolValue];
	spx_int32_t agc_enabled = [[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexEnableAGC] boolValue];
	if(denoise_enabled || agc_enabled) {
		_preprocess = speex_preprocess_state_init(_speex_frame_size, rate);
		speex_preprocess_ctl(_preprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoise_enabled);
		speex_preprocess_ctl(_preprocess, SPEEX_PREPROCESS_SET_AGC, &agc_enabled);
		_speex_lookahead += _speex_frame_size;
	}

	// Write stream headers
	SpeexHeader header;
	speex_init_header(&header, (int)_processingFormat.sampleRate, (int)_processingFormat.channelCount, speex_mode);

	_speex_frames_per_ogg_packet = 1;  //1-10 default 1
	NSNumber *framesPerPacket = [_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexSpeexFramesPerOggPacket] ?: @1;
	if(framesPerPacket != nil) {
		int intValue = framesPerPacket.intValue;
		if(intValue < 1 || intValue > 10) {
			os_log_error(gSFBAudioEncoderLog, "Invalid Ogg Speex frames per packet: %d", intValue);
			ogg_stream_clear(&_os);
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
			return NO;
		}
		_speex_frames_per_ogg_packet = intValue;
	}

	header.frames_per_packet = _speex_frames_per_ogg_packet;
	header.vbr = [[_settings objectForKey:SFBAudioEncodingSettingsKeyOggSpeexEnableVBR] boolValue];
	header.nb_channels = (spx_int32_t)_processingFormat.channelCount;

	int packet_size;
	unsigned char *packet_data = (unsigned char *)speex_header_to_packet(&header, &packet_size);

	ogg_packet op;
	op.packet = packet_data;
	op.bytes = packet_size;
	op.b_o_s = 1;
	op.e_o_s = 0;
	op.granulepos = 0;
	op.packetno = 0;

	ogg_stream_packetin(&_os, &op);
	speex_header_free(packet_data);

	for(;;) {
		ogg_page og;
		if(ogg_stream_pageout(&_os, &og) == 0)
			break;

		NSInteger bytesWritten;
		if(![_outputSource writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] || bytesWritten != og.header_len) {
			speex_encoder_destroy(_st);
			ogg_stream_clear(&_os);
			return NO;
		}

		if(![_outputSource writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] || bytesWritten != og.body_len) {
			speex_encoder_destroy(_st);
			ogg_stream_clear(&_os);
			return NO;
		}

		if(ogg_page_eos(&og))
			break;
	}

	const char *speex_version;
	char vendor_string [64];
	speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void *)&speex_version);
	snprintf(vendor_string, sizeof(vendor_string), "Encoded with Speex %s", speex_version);

	char *comments;
	size_t comments_length;
	vorbis_comment_init(&comments, &comments_length, vendor_string);

	op.packet = (unsigned char *)comments;
	op.bytes = (long)comments_length;
	op.b_o_s = 0;
	op.e_o_s = 0;
	op.granulepos = 0;
	op.packetno = 1;

	ogg_stream_packetin(&_os, &op);
	free(comments);

	for(;;) {
		ogg_page og;
		if(ogg_stream_pageout(&_os, &og) == 0)
			break;

		NSInteger bytesWritten;
		if(![_outputSource writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] || bytesWritten != og.header_len) {
			speex_encoder_destroy(_st);
			ogg_stream_clear(&_os);
			return NO;
		}

		if(![_outputSource writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] || bytesWritten != og.body_len) {
			speex_encoder_destroy(_st);
			ogg_stream_clear(&_os);
			return NO;
		}

		if(ogg_page_eos(&og))
			break;
	}

	speex_bits_init(&_bits);

	_speex_frame_number = -1;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	speex_bits_destroy(&_bits);
	memset(&_bits, 0, sizeof(SpeexBits));
	if(_st) {
		speex_encoder_destroy(_st);
		_st = NULL;
	}
	if(_preprocess) {
		speex_preprocess_state_destroy(_preprocess);
		_preprocess = NULL;
	}
	ogg_stream_clear(&_os);

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _st != NULL;
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

	// Split buffer into Speex frame-sized chunks
	AVAudioFrameCount framesProcessed = 0;

	for(;;) {
		AVAudioFrameCount framesCopied = [_frameBuffer appendContentsOfBuffer:buffer readOffset:framesProcessed];
		framesProcessed += framesCopied;

		// Encode the next Speex frame
		if(_frameBuffer.isFull) {
			if(![self encodeSpeexFrameReturningError:error])
				return NO;
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
	// Encode any remaining audio
	if(!_frameBuffer.isEmpty && ![self encodeSpeexFrameReturningError:error])
		return NO;
	_frameBuffer.frameLength = 0;

	char cbits [MAX_FRAME_BYTES];

	// Finish up
	if(((_speex_frame_number + 1) % _speex_frames_per_ogg_packet) != 0) {
		while(((_speex_frame_number + 1) % _speex_frames_per_ogg_packet) != 0) {
			++_speex_frame_number;
			speex_bits_pack(&_bits, 15, 5);
		}

		int byte_count = speex_bits_write(&_bits, cbits, MAX_FRAME_BYTES);

		ogg_packet op;
		op.packet = (unsigned char *)cbits;
		op.bytes = byte_count;
		op.b_o_s = 0;
		op.e_o_s = 1;
		op.granulepos = ((_speex_frame_number + 1) * _speex_frame_size) - _speex_lookahead;
		if(op.granulepos > _framePosition)
			op.granulepos = _framePosition;

		op.packetno = 2 + (_speex_frame_number / _speex_frames_per_ogg_packet);
		ogg_stream_packetin(&_os, &op);
	}

	// Flush all pages left to be written
	for(;;) {
		ogg_page og;
		if(ogg_stream_flush(&_os, &og) == 0)
			break;

		NSInteger bytesWritten;
		if(![_outputSource writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] || bytesWritten != og.header_len)
			return NO;

		if(![_outputSource writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] || bytesWritten != og.body_len)
			return NO;
	}

	return YES;
}

- (BOOL)encodeSpeexFrameReturningError:(NSError **)error
{
	AVAudioFrameCount framesOfSilenceAdded = 0;
	if(!_frameBuffer.isFull)
		framesOfSilenceAdded = [_frameBuffer fillRemainderWithSilence];

	if(_processingFormat.channelCount == 2)
		speex_encode_stereo_int(_frameBuffer.audioBufferList->mBuffers[0].mData, (int)_frameBuffer.frameLength, &_bits);
	if(_preprocess)
		speex_preprocess(_preprocess, _frameBuffer.audioBufferList->mBuffers[0].mData, NULL);
	speex_encode_int(_st, _frameBuffer.audioBufferList->mBuffers[0].mData, &_bits);

	_framePosition += _frameBuffer.frameLength - framesOfSilenceAdded;

	++_speex_frame_number;

	// Emit ogg packet
	if(((_speex_frame_number + 1) % _speex_frames_per_ogg_packet) == 0) {
		char cbits [MAX_FRAME_BYTES];

		speex_bits_insert_terminator(&_bits);
		int byte_count = speex_bits_write(&_bits, cbits, MAX_FRAME_BYTES);
		speex_bits_reset(&_bits);

		ogg_packet op;
		op.packet = (unsigned char *)cbits;
		op.bytes = byte_count;
		op.b_o_s = 0;
		op.e_o_s = (framesOfSilenceAdded > 0);
		op.granulepos = ((_speex_frame_number + 1) * _speex_frame_size) - _speex_lookahead;
		if(op.granulepos > _framePosition)
			op.granulepos = _framePosition;

		op.packetno = 2 + (_speex_frame_number / _speex_frames_per_ogg_packet);

		ogg_stream_packetin(&_os, &op);

		for(;;) {
			ogg_page og;
			if(ogg_stream_pageout(&_os, &og) == 0)
				break;

			NSInteger bytesWritten;
			if(![_outputSource writeBytes:og.header length:og.header_len bytesWritten:&bytesWritten error:error] || bytesWritten != og.header_len) {
				return NO;
			}

			if(![_outputSource writeBytes:og.body length:og.body_len bytesWritten:&bytesWritten error:error] || bytesWritten != og.body_len) {
				return NO;
			}

			if(ogg_page_eos(&og))
				break;
		}
	}

	return YES;
}

@end
