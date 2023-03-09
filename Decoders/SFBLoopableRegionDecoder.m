//
// Copyright (c) 2006 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;

#import "SFBLoopableRegionDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"
#import "SFBAudioDecoder+Internal.h"

@interface SFBLoopableRegionDecoder ()
{
@private
	id <SFBPCMDecoding> _decoder;
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

- (instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithURL:url framePosition:framePosition frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithURL:(NSURL *)url framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource || !inputSource.supportsSeeking)
		return nil;
	return [self initWithInputSource:inputSource framePosition:framePosition frameLength:frameLength repeatCount:repeatCount error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithInputSource:inputSource framePosition:framePosition frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithInputSource:inputSource error:error];
	if(!decoder || !decoder.supportsSeeking)
		return nil;
	return [self initWithDecoder:decoder framePosition:framePosition frameLength:frameLength repeatCount:repeatCount error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithDecoder:decoder framePosition:framePosition frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithDecoder:(id <SFBPCMDecoding>)decoder framePosition:(AVAudioFramePosition)framePosition frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(framePosition >= 0);
	NSParameterAssert(frameLength >= 0);
	NSParameterAssert(repeatCount >= 0);

	if((self = [super init])) {
		_decoder = decoder;
		_framePosition = framePosition;
		_frameLength = frameLength;
		_repeatCount = repeatCount;
	}
	return self;
}

- (SFBInputSource *)inputSource
{
	return _decoder.inputSource;
}

- (AVAudioFormat *)processingFormat
{
	return _decoder.processingFormat;
}

- (AVAudioFormat *)sourceFormat
{
	return _decoder.sourceFormat;
}

- (BOOL)decodingIsLossless
{
	return _decoder.decodingIsLossless;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(!_decoder.isOpen && ![_decoder openReturningError:error])
		return NO;

	if(!_decoder.supportsSeeking || ![self setupDecoderForcingReset:NO error:error]) {
		[_decoder closeReturningError:error];
		return NO;
	}

	_buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_decoder.processingFormat frameCapacity:512];

	return YES;
}

- (BOOL)closeReturningError:(NSError **)error
{
	_buffer = nil;
	return [_decoder closeReturningError:error];
}

- (BOOL)isOpen
{
	return _buffer != nil;
}

- (AVAudioFramePosition)framePosition
{
	return _framesDecoded;
}

- (AVAudioFramePosition)frameLength
{
	return _frameLength * (_repeatCount + 1);
}

- (BOOL)decodeIntoBuffer:(AVAudioBuffer *)buffer error:(NSError **)error {
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer isKindOfClass:[AVAudioPCMBuffer class]]);
	return [self decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:((AVAudioPCMBuffer *)buffer).frameCapacity error:error];
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error
{
	NSParameterAssert(buffer != nil);
	NSParameterAssert([buffer.format isEqual:_decoder.processingFormat]);

	// Reset output buffer data size
	buffer.frameLength = 0;

	if((_repeatCount && (_framesDecoded / _frameLength) == (_repeatCount + 1)) || frameLength == 0)
		return YES;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	AVAudioFrameCount framesRemaining = frameLength;

	_buffer.frameLength = 0;

	while(framesRemaining > 0) {
		AVAudioFrameCount framesRemainingInCurrentPass = (AVAudioFrameCount)(_framePosition + _frameLength - _decoder.framePosition);
		AVAudioFrameCount framesToDecode = MIN(MIN(framesRemaining, framesRemainingInCurrentPass), _buffer.frameCapacity);

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
		if( _repeatCount ) {
			if ( _framesDecoded > 0 && _framesDecoded % _frameLength == 0 ) {
				// Only seek to the beginning of the region if more passes remain
				if((_framesDecoded / _frameLength) < _repeatCount + 1 ) {
					if(![_decoder seekToFrame:_framePosition error:error])
						return NO;
				}
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
