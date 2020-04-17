/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBLoopableRegionDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "SFBAudioDecoder+Internal.h"

static inline AVAudioFrameCount SFB_min(AVAudioFrameCount a, AVAudioFrameCount b) { return a < b ? a : b; }

@interface SFBLoopableRegionDecoder ()
{
@private
	SFBAudioDecoder *_decoder;
	AVAudioPCMBuffer *_buffer;
	AVAudioFramePosition _framePosition;
	AVAudioFramePosition _frameLength;
	NSInteger _repeatCount;
	AVAudioFramePosition _framesDecoded;
}
- (BOOL)resetReturningError:(NSError **)error;
- (BOOL)setupDecoderForcingReset:(BOOL)forceReset error:(NSError **)error;
@end

@implementation SFBLoopableRegionDecoder

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"

// This is only here because it's the SFBAudioDecoder designated initializer
- (instancetype)initWithInputSource:(SFBInputSource *)inputSource mimeType:(NSString *)mimeType error:(NSError **)error
{
	[self doesNotRecognizeSelector:_cmd];
	__builtin_unreachable();
}

#pragma clang diagnostic pop

- (instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithURL:url framePosition:framePosition frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource || !inputSource.supportsSeeking)
		return nil;
	return [self initWithInputSource:inputSource framePosition:framePosition frameLength:frameLength repeatCount:repeatCount error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithInputSource:inputSource framePosition:framePosition frameLength:frameLength repeatCount:0 error:error];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-designated-initializers"

// This is not pretty... no call to [super init], but this class is an oddball
// It might make sense to abtract out the methods from SFBAudioDecoder into
// a protocol (SFBAudioDecoding), and have this class implement that and not
// inherit from SFBAudioDecoder.
// That would drive SFBAudioPlayer changing to accept id <SFBAudioDecoding>
// instead of an SFBAudioDecoder instance
- (instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(framePosition >= 0);
	NSParameterAssert(frameLength >= 0);
	NSParameterAssert(repeatCount >= 0);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithInputSource:inputSource error:error];
	if(!decoder || !decoder.supportsSeeking)
		return nil;

	_decoder = decoder;
	_framePosition = framePosition;
	_frameLength = frameLength;
	_repeatCount = repeatCount;

	return self;
}

#pragma clang diagnostic pop

- (SFBInputSource *)inputSource
{
	return _decoder->_inputSource;
}

- (AVAudioFormat *)processingFormat
{
	return _decoder->_processingFormat;
}

- (AVAudioFormat *)sourceFormat
{
	return _decoder->_sourceFormat;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-missing-super-calls"

- (BOOL)openReturningError:(NSError **)error
{
	if(!_decoder.isOpen && ![_decoder openReturningError:error])
		return NO;

	if(!_decoder.supportsSeeking || ![self setupDecoderForcingReset:NO error:error]) {
		[_decoder closeReturningError:error];
		return NO;
	}

	_buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_decoder->_processingFormat frameCapacity:512];

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_buffer = nil;
	return [_decoder closeReturningError:error];
}

#pragma clang diagnostic pop

- (BOOL)isOpen
{
	return _decoder.isOpen;
}

- (AVAudioFramePosition)framePosition
{
	return _framesDecoded;
}

- (AVAudioFramePosition)frameLength
{
	return _frameLength * (_repeatCount + 1);
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

	if(_repeatCount && (_framesDecoded / _frameLength) == (_repeatCount + 1))
		return YES;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	AVAudioFrameCount framesRemaining = frameLength;

	_buffer.frameLength = 0;

	while(framesRemaining > 0) {
		AVAudioFrameCount framesRemainingInCurrentPass	= (AVAudioFrameCount)(_framePosition + _frameLength - _decoder.framePosition);
		AVAudioFrameCount framesToDecode				= SFB_min(SFB_min(framesRemaining, framesRemainingInCurrentPass), _buffer.frameCapacity);

		// Nothing left to read
		if(framesToDecode == 0)
			break;

		// Zero the internal buffer in preparation for decoding
		_buffer.frameLength = 0;

		// Decode audio into our internal buffer and append it to output
		if(![_decoder decodeIntoBuffer:_buffer frameLength:framesToDecode error:error])
			return NO;

		[buffer appendContentsOfBuffer:_buffer];

		// Housekeeping
		_framesDecoded += _buffer.frameLength;
		framesRemaining -= _buffer.frameLength;

		// If this pass is finished, seek to the beginning of the region in preparation for the next read
		if(_repeatCount && _frameLength == (_framesDecoded / _frameLength)) {
			// Only seek to the beginning of the region if more passes remain
			if((_framesDecoded / _frameLength) < (_repeatCount + 1)) {
				if(![_decoder seekToFrame:_framePosition error:error])
					return NO;
			}
		}
	}

	return YES;
}

- (BOOL)supportsSeeking
{
	return _decoder.supportsSeeking;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error
{
	NSParameterAssert(frame >= 0);

	if(frame >= self.frameLength)
		return NO;

	_framesDecoded = frame;
	return [_decoder seekToFrame:(_framePosition + (frame % _frameLength)) error:error];
}

- (BOOL)resetReturningError:(NSError **)error
{
	_framesDecoded = 0;

	if(_framePosition == _decoder.framePosition)
		return YES;

	if(![_decoder seekToFrame:_framePosition error:error])
		return NO;

	return _framePosition == _decoder.framePosition;
}

- (BOOL)setupDecoderForcingReset:(BOOL)forceReset error:(NSError **)error
{
	if(_frameLength == 0)
		_frameLength = _decoder.frameLength - _framePosition;

	if(forceReset || _framePosition != 0)
		return [self resetReturningError:error];

	return YES;
}

@end