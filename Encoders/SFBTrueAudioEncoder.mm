/*
 * Copyright (c) 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <memory>

#include <tta++/libtta.h>

#import "SFBTrueAudioEncoder.h"

SFBAudioEncoderName const SFBAudioEncoderNameTrueAudio = @"org.sbooth.AudioEngine.Encoder.TrueAudio";

namespace {

	struct TTACallbacks : TTA_io_callback
	{
		SFBAudioEncoder *mEncoder;
	};

	TTAint32 write_callback(struct _tag_TTA_io_callback *io, TTAuint8 *buffer, TTAuint32 size)
	{
		TTACallbacks *iocb = (TTACallbacks *)io;

		NSInteger bytesWritten;
		if(![iocb->mEncoder->_outputSource writeBytes:buffer length:size bytesWritten:&bytesWritten error:nil])
			return -1;
		return (TTAint32)bytesWritten;
	}

	TTAint64 seek_callback(struct _tag_TTA_io_callback *io, TTAint64 offset)
	{
		TTACallbacks *iocb = (TTACallbacks *)io;

		if(![iocb->mEncoder->_outputSource seekToOffset:offset error:nil])
			return -1;
		return offset;
	}

}

@interface SFBTrueAudioEncoder ()
{
@private
	std::unique_ptr<tta::tta_encoder> _encoder;
	std::unique_ptr<TTACallbacks> _callbacks;
	AVAudioFramePosition _framePosition;
}
@end

@implementation SFBTrueAudioEncoder

+ (void)load
{
	[SFBAudioEncoder registerSubclass:[self class]];
}

+ (NSSet *)supportedPathExtensions
{
	return [NSSet setWithObject:@"tta"];
}

+ (NSSet *)supportedMIMETypes
{
	return [NSSet setWithObject:@"audio/x-tta"];
}

+ (SFBAudioEncoderName)encoderName
{
	return SFBAudioEncoderNameTrueAudio;
}

- (BOOL)encodingIsLossless
{
	return YES;
}

- (AVAudioFormat *)processingFormatForSourceFormat:(AVAudioFormat *)sourceFormat
{
	NSParameterAssert(sourceFormat != nil);

	// Validate format
	if(sourceFormat.streamDescription->mFormatFlags & kAudioFormatFlagIsFloat || sourceFormat.streamDescription->mBitsPerChannel < MIN_BPS || sourceFormat.streamDescription->mBitsPerChannel > MAX_BPS || sourceFormat.channelCount < 1 || sourceFormat.channelCount > MAX_NCH)
		return nil;

	// Set up the processing format
	AudioStreamBasicDescription streamDescription{};

	streamDescription.mFormatID				= kAudioFormatLinearPCM;
	streamDescription.mFormatFlags			= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger;

	streamDescription.mSampleRate			= sourceFormat.sampleRate;
	streamDescription.mChannelsPerFrame		= sourceFormat.channelCount;
	streamDescription.mBitsPerChannel		= sourceFormat.streamDescription->mBitsPerChannel;
	if(streamDescription.mBitsPerChannel == 16 || streamDescription.mBitsPerChannel == 24)
		streamDescription.mFormatFlags		|= kAudioFormatFlagIsPacked;

	streamDescription.mBytesPerPacket		= ((sourceFormat.streamDescription->mBitsPerChannel + 7) / 8) * streamDescription.mChannelsPerFrame;
	streamDescription.mFramesPerPacket		= 1;
	streamDescription.mBytesPerFrame		= streamDescription.mBytesPerPacket / streamDescription.mFramesPerPacket;

	// TODO: what channel layout is appropriate?
	AVAudioChannelLayout *channelLayout = nil;
	return [[AVAudioFormat alloc] initWithStreamDescription:&streamDescription channelLayout:channelLayout];
}

- (BOOL)openReturningError:(NSError **)error
{
	if(![super openReturningError:error])
		return NO;

	// True Audio requires knowing the number of frames to encode in advance
	if(_estimatedFramesToEncode <= 0) {
		os_log_error(gSFBAudioEncoderLog, "True Audio encoding requires an accurate value for _estimatedFramesToEncode");
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	_callbacks				= std::make_unique<TTACallbacks>();
	_callbacks->read		= nullptr;
	_callbacks->write		= write_callback;
	_callbacks->seek		= seek_callback;
	_callbacks->mEncoder	= self;

	TTA_info streamInfo;

	streamInfo.format = TTA_FORMAT_SIMPLE;
	streamInfo.nch = _processingFormat.channelCount;
	streamInfo.bps = _processingFormat.streamDescription->mBitsPerChannel;
	streamInfo.sps = (TTAuint32)_processingFormat.sampleRate;
	streamInfo.samples = (TTAuint32)_estimatedFramesToEncode;

	try {
		_encoder = std::make_unique<tta::tta_encoder>((TTA_io_callback *)_callbacks.get());
		_encoder->init_set_info(&streamInfo, 0);
	}
	catch(const tta::tta_exception& e) {
		os_log_error(gSFBAudioEncoderLog, "Error creating True Audio encoder: %d", e.code());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:nil];
		return NO;
	}

	if(!_encoder) {
		if(error)
			if(error)
				*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInvalidFormat userInfo:nil];
		return NO;
	}

	AudioStreamBasicDescription outputStreamDescription{};
	outputStreamDescription.mFormatID			= kSFBAudioFormatTrueAudio;
	outputStreamDescription.mBitsPerChannel		= _processingFormat.streamDescription->mBitsPerChannel;
	outputStreamDescription.mSampleRate			= _processingFormat.sampleRate;
	outputStreamDescription.mChannelsPerFrame	= _processingFormat.channelCount;
	_outputFormat = [[AVAudioFormat alloc] initWithStreamDescription:&outputStreamDescription];

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_encoder.reset();
	_callbacks.reset();

	return [super closeReturningError:error];
}

- (BOOL)isOpen
{
	return _encoder != nullptr;
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

	try {
		auto bytesToWrite = frameLength * _processingFormat.streamDescription->mBytesPerFrame;
		_encoder->process_stream((TTAuint8 *)buffer.audioBufferList->mBuffers[0].mData, bytesToWrite);
	}

	catch(const tta::tta_exception& e) {
		os_log_error(gSFBAudioEncoderLog, "_encoder->process_stream() failed: %d", e.code());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	_framePosition += frameLength;

	return YES;
}

- (BOOL)finishEncodingReturningError:(NSError **)error
{
	try {
		_encoder->finalize();
	}

	catch(const tta::tta_exception& e) {
		os_log_error(gSFBAudioEncoderLog, "_encoder->finalize() failed: %d", e.code());
		if(error)
			*error = [NSError errorWithDomain:SFBAudioEncoderErrorDomain code:SFBAudioEncoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	return YES;
}

@end
