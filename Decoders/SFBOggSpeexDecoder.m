/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#include <Accelerate/Accelerate.h>

#include <ogg/ogg.h>
#include <speex/speex.h>
#include <speex/speex_bits.h>
#include <speex/speex_callbacks.h>
#include <speex/speex_header.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"

#include <speex/speex_stereo.h>

#pragma clang diagnostic pop

#import "SFBOggSpeexDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "NSError+SFBURLPresentation.h"

#define MAX_FRAME_SIZE 2000
#define READ_SIZE_BYTES 4096

@interface SFBOggSpeexDecoder ()
{
@private
	AVAudioPCMBuffer *_buffer;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;

	ogg_sync_state _syncState;
	ogg_page _page;
	ogg_stream_state _streamState;

	void *_decoder;
	SpeexBits _bits;
	SpeexStereoState *_stereoState;

	long _serialNumber;
	BOOL _eosReached;
	spx_int32_t _framesPerOggPacket;
	NSInteger _oggPacketCount;
	NSInteger _extraSpeexHeaderCount;
}
@end

@implementation SFBOggSpeexDecoder

+ (void)load
{
	[SFBAudioDecoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"spx"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithArray:@[@"audio/speex", @"audio/ogg"]];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(NSString *)mimeType error:(NSError **)error
{
	if((self = [super initWithInputSource:inputSource mimeType:mimeType error:error])) {
		_frameLength = -1;
		_serialNumber = -1;
	}
	return self;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	// Initialize Ogg data struct
	ogg_sync_init(&_syncState);

	// Get the ogg buffer for writing
	char *data = ogg_sync_buffer(&_syncState, READ_SIZE_BYTES);

	// Read bitstream from input file
	NSInteger bytesRead;
	if(![_inputSource readBytes:data length:READ_SIZE_BYTES bytesRead:&bytesRead error:error]) {
		ogg_sync_destroy(&_syncState);
		return NO;
	}

	// Tell the sync layer how many bytes were written to its internal buffer
	int result = ogg_sync_wrote(&_syncState, bytesRead);
	if(result == -1) {
		ogg_sync_destroy(&_syncState);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Ogg file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not an Ogg file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Turn the data we wrote into an ogg page
	result = ogg_sync_pageout(&_syncState, &_page);
	if(result != 1) {
		ogg_sync_destroy(&_syncState);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Ogg file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not an Ogg file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Initialize the stream and grab the serial number
	ogg_stream_init(&_streamState, ogg_page_serialno(&_page));

	// Get the first Ogg page
	result = ogg_stream_pagein(&_streamState, &_page);
	if(result) {
		ogg_sync_destroy(&_syncState);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Ogg file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not an Ogg file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	// Get the first packet (should be the header) from the page
	ogg_packet op;
	result = ogg_stream_packetout(&_streamState, &op);
	if(result != 1) {
		ogg_sync_destroy(&_syncState);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Ogg file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not an Ogg file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];

		return NO;
	}

	if(op.bytes >= 5 && !memcmp(op.packet, "Speex", 5))
		_serialNumber = _streamState.serialno;

	++_oggPacketCount;

	// Convert the packet to the Speex header
	SpeexHeader *header = speex_packet_to_header((char *)op.packet, (int)op.bytes);
	if(!header) {
		ogg_sync_destroy(&_syncState);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The file “%@” is not a valid Ogg Speex file.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Not an Ogg Speex file", @"")
							   recoverySuggestion:NSLocalizedString(@"The file's extension may not match the file's type.", @"")];
		return NO;
	}
	else if(header->mode >= SPEEX_NB_MODES) {
		speex_header_free(header);
		ogg_sync_destroy(&_syncState);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The Speex mode in the file “%@” is not supported.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported Ogg Speex file mode", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been encoded with a newer version of Speex.", @"")];

		return NO;
	}

	const SpeexMode *mode = speex_lib_get_mode(header->mode);
	if(mode->bitstream_version != header->mode_bitstream_version) {
		speex_header_free(header);
		ogg_sync_destroy(&_syncState);

		if(error)
			*error = [NSError SFB_errorWithDomain:SFBAudioDecoderErrorDomain
											 code:SFBAudioDecoderErrorCodeInputOutput
					descriptionFormatStringForURL:NSLocalizedString(@"The Speex version in the file “%@” is not supported.", @"")
											  url:_inputSource.url
									failureReason:NSLocalizedString(@"Unsupported Ogg Speex file version", @"")
							   recoverySuggestion:NSLocalizedString(@"The file may have been encoded with a newer version of Speex.", @"")];

		return NO;
	}

	// Initialize the decoder
	_decoder = speex_decoder_init(mode);
	if(!_decoder) {
		speex_header_free(header);
		ogg_sync_destroy(&_syncState);

		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain
										 code:SFBAudioDecoderErrorCodeInputOutput
									 userInfo:@{
										 NSLocalizedDescriptionKey: NSLocalizedString(@"Unable to initialize the Speex decoder.", @""),
										 NSLocalizedFailureReasonErrorKey: NSLocalizedString(@"Error initializing Speex decoder", @""),
										 NSLocalizedRecoverySuggestionErrorKey: NSLocalizedString(@"An unknown error occurred.", @"")
									 }];

		return NO;
	}

	speex_decoder_ctl(_decoder, SPEEX_SET_SAMPLING_RATE, &header->rate);

	_framesPerOggPacket = (header->frames_per_packet == 0 ? 1 : header->frames_per_packet);
	_extraSpeexHeaderCount = header->extra_headers;

	// Initialize the speex bit-packing data structure
	speex_bits_init(&_bits);

	// Initialize the stereo mode
	_stereoState = speex_stereo_state_init();

	if(header->nb_channels == 2) {
		SpeexCallback callback;
		callback.callback_id = SPEEX_INBAND_STEREO;
		callback.func = speex_std_stereo_request_handler;
		callback.data = _stereoState;
		speex_decoder_ctl(_decoder, SPEEX_SET_HANDLER, &callback);
	}

//	AVAudioChannelLayout *channelLayout = nil;
//	switch(header->nb_channels) {
//		case 1:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Mono];			break;
//		case 2:		channelLayout = [AVAudioChannelLayout layoutWithLayoutTag:kAudioChannelLayoutTag_Stereo];		break;
//	}

	// For mono and stereo the channel layout is assumed
	_processingFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32 sampleRate:header->rate channels:(AVAudioChannelCount)header->nb_channels interleaved:NO];

	// Set up the source format
	AudioStreamBasicDescription sourceStreamDescription = {0};

	sourceStreamDescription.mFormatID			= SFBAudioFormatIDSpeex;

	sourceStreamDescription.mSampleRate			= header->rate;
	sourceStreamDescription.mChannelsPerFrame	= (UInt32)header->nb_channels;

	_sourceFormat = [[AVAudioFormat alloc] initWithStreamDescription:&sourceStreamDescription];

	speex_header_free(header);

	// Allocate the buffer list
	spx_int32_t speexFrameSize = 0;
	speex_decoder_ctl(_decoder, SPEEX_GET_FRAME_SIZE, &speexFrameSize);

	_buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:(AVAudioFrameCount)speexFrameSize];
	_buffer.frameLength = 0;

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	// Speex cleanup
	if(_stereoState) {
		speex_stereo_state_destroy(_stereoState);
		_stereoState = NULL;
	}

	if(_decoder) {
		speex_decoder_destroy(_decoder);
		_decoder = NULL;
	}

	speex_bits_destroy(&_bits);

	// Ogg cleanup
	ogg_stream_clear(&_streamState);
	ogg_sync_clear(&_syncState);

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _decoder != NULL;
}

- (AVAudioFramePosition)framePosition
{
	return _framePosition;
}

- (AVAudioFramePosition)frameLength
{
	return _frameLength;
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if(![buffer.format isEqual:_processingFormat]) {
		os_log_debug(OS_LOG_DEFAULT, "-decodeAudio:frameLength:error: called with invalid parameters");
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

		// EOS reached
		if(_eosReached)
			break;

		// Attempt to process the desired number of packets
		NSInteger packetsDesired = 1;
		while(packetsDesired > 0 && !_eosReached) {

			// Process any packets in the current page
			while(packetsDesired > 0 && !_eosReached) {

				// Grab a packet from the streaming layer
				ogg_packet oggPacket;
				int result = ogg_stream_packetout(&_streamState, &oggPacket);
				if(result == -1) {
					os_log_error(OS_LOG_DEFAULT, "Ogg Speex decoding error: Ogg loss of streaming");
					break;
				}

				// If result is 0, there is insufficient data to assemble a packet
				if(result == 0)
					break;

				// Otherwise, we got a valid packet for processing
				if(result == 1) {
					if(oggPacket.bytes >= 5 && !memcmp(oggPacket.packet, "Speex", 5))
						_serialNumber = _streamState.serialno;

					if(_serialNumber == -1 || _streamState.serialno != _serialNumber)
						break;

					// Ignore the following:
					//  - Speex comments in packet #2
					//  - Extra headers (optionally) in packets 3+
					if(_oggPacketCount != -1 && _extraSpeexHeaderCount + 1 <= _oggPacketCount) {
						// Detect Speex EOS
						if(oggPacket.e_o_s && _streamState.serialno == _serialNumber)
							_eosReached = YES;

						// SPEEX_GET_FRAME_SIZE is in samples
						spx_int32_t speexFrameSize;
						speex_decoder_ctl(_decoder, SPEEX_GET_FRAME_SIZE, &speexFrameSize);
						float buf [speexFrameSize * _processingFormat.channelCount];

						// Copy the Ogg packet to the Speex bitstream
						speex_bits_read_from(&_bits, (char *)oggPacket.packet, (int)oggPacket.bytes);

						// Decode each frame in the Speex packet
						for(spx_int32_t i = 0; i < _framesPerOggPacket; ++i) {
							result = speex_decode(_decoder, &_bits, buf);

							// -1 indicates EOS
							if(result == -1)
								break;
							else if(result == -2) {
								os_log_error(OS_LOG_DEFAULT, "Ogg Speex decoding error: possible corrupted stream");
								break;
							}

							if(speex_bits_remaining(&_bits) < 0) {
								os_log_error(OS_LOG_DEFAULT, "Ogg Speex decoding overflow: possible corrupted stream");
								break;
							}

							// Normalize the values
							float maxSampleValue = 1u << 15;
							vDSP_vsdiv(buf, 1, &maxSampleValue, buf, 1, (vDSP_Length)speexFrameSize);

							// Copy the frames from the decoding buffer to the output buffer, skipping over any frames already decoded

							float * const *floatChannelData = _buffer.floatChannelData;

							const float *input = buf;
							float *output = floatChannelData[0] + _buffer.frameLength;
							memcpy(output, input, (size_t)speexFrameSize * sizeof(float));

							// Process stereo channel, if present
							if(_processingFormat.channelCount == 2) {
								speex_decode_stereo(buf, speexFrameSize, _stereoState);
								vDSP_vsdiv(buf + speexFrameSize, 1, &maxSampleValue, buf + speexFrameSize, 1, (vDSP_Length)speexFrameSize);

								input = buf + speexFrameSize;
								output = floatChannelData[1] + _buffer.frameLength;
								memcpy(output, input, (size_t)speexFrameSize * sizeof(float));
							}

							_buffer.frameLength = (AVAudioFrameCount)speexFrameSize;

							// Packet processing finished
							--packetsDesired;
						}
					}

					++_oggPacketCount;
				}
			}

			// Grab a new Ogg page for processing, if necessary
			if(!_eosReached && packetsDesired > 0) {
				while(ogg_sync_pageout(&_syncState, &_page) != 1) {
					// Get the ogg buffer for writing
					char *data = ogg_sync_buffer(&_syncState, READ_SIZE_BYTES);

					// Read bitstream from input file
					NSInteger bytesRead;
					if(![_inputSource readBytes:data length:READ_SIZE_BYTES bytesRead:&bytesRead error:nil]) {
						os_log_error(OS_LOG_DEFAULT, "Unable to read from the input file");
						break;
					}

					ogg_sync_wrote(&_syncState, bytesRead);

					// No more data available from input file
					if(bytesRead == 0)
						break;
				}

				// Ensure all Ogg streams are read
				if(ogg_page_serialno(&_page) != _streamState.serialno)
					ogg_stream_reset_serialno(&_streamState, ogg_page_serialno(&_page));

				// Get the resultant Ogg page
				int result = ogg_stream_pagein(&_streamState, &_page);
				if(result) {
					os_log_error(OS_LOG_DEFAULT, "Error reading Ogg page");
					break;
				}
			}
		}
	}

	_framePosition += framesProcessed;

	if(framesProcessed == 0 && _eosReached)
		_frameLength = _framePosition;

	return YES;
}

- (BOOL)supportsSeeking
{
	return NO;
}

@end
