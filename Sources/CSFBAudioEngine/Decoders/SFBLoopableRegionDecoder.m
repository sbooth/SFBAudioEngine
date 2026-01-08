//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;
@import stdlib_h;

@import AVFAudioExtensions;

#import "SFBLoopableRegionDecoder.h"

#import "SFBAudioDecoder+Internal.h"

@interface SFBLoopableRegionDecoder ()
{
@private
	id<SFBPCMDecoding> _decoder;
	AVAudioPCMBuffer *_buffer;
	AVAudioFramePosition _startFrame;
	AVAudioFramePosition _endFrame;
}
@end

@implementation SFBLoopableRegionDecoder

- (instancetype)initWithURL:(NSURL *)url startingFrame:(AVAudioFramePosition)startingFrame error:(NSError **)error
{
	return [self initWithURL:url startingFrame:startingFrame frameLength:-1 repeatCount:0 error:error];
}

- (instancetype)initWithURL:(NSURL *)url frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithURL:url startingFrame:0 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithURL:(NSURL *)url startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithURL:url startingFrame:startingFrame frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithURL:(NSURL *)url repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	return [self initWithURL:url startingFrame:0 frameLength:-1 repeatCount:repeatCount error:error];
}

- (instancetype)initWithURL:(NSURL *)url startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource)
		return nil;
	return [self initWithInputSource:inputSource startingFrame:startingFrame frameLength:frameLength repeatCount:repeatCount error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource startingFrame:(AVAudioFramePosition)startingFrame error:(NSError **)error
{
	return [self initWithInputSource:inputSource startingFrame:startingFrame frameLength:-1 repeatCount:0 error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithInputSource:inputSource startingFrame:0 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithInputSource:inputSource startingFrame:startingFrame frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	return [self initWithInputSource:inputSource startingFrame:0 frameLength:-1 repeatCount:repeatCount error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithInputSource:inputSource error:error];
	if(!decoder)
		return nil;
	return [self initWithDecoder:decoder startingFrame:startingFrame frameLength:frameLength repeatCount:repeatCount error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder startingFrame:(AVAudioFramePosition)startingFrame error:(NSError **)error
{
	return [self initWithDecoder:decoder startingFrame:startingFrame frameLength:-1 repeatCount:0 error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithDecoder:decoder startingFrame:0 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithDecoder:decoder startingFrame:startingFrame frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	return [self initWithDecoder:decoder startingFrame:0 frameLength:-1 repeatCount:repeatCount error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder startingFrame:(AVAudioFramePosition)startingFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(startingFrame >= 0);
	NSParameterAssert(frameLength > 0 || frameLength == -1);
	NSParameterAssert(repeatCount >= -1);

	if((self = [super init])) {
		_decoder = decoder;
		_regionStartingFrame = startingFrame;
		_regionFrameLength = frameLength;
		_repeatCount = repeatCount;
		_completedLoops = 0;
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

- (NSDictionary *)properties
{
	return _decoder.properties;
}

- (BOOL)openReturningError:(NSError **)error
{
	if(!_decoder.isOpen && ![_decoder openReturningError:error])
		return NO;

	if(!_decoder.supportsSeeking) {
		os_log_error(gSFBAudioDecoderLog, "Cannot open LoopableRegionDecoder with non-seekable decoder %{public}@", _decoder);
		[_decoder closeReturningError:nil];
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	if(_decoder.framePosition != _regionStartingFrame && ![_decoder seekToFrame:_regionStartingFrame error:error]) {
		[_decoder closeReturningError:nil];
		return NO;
	}

	_startFrame = _regionStartingFrame;

	AVAudioFramePosition frameLength = _decoder.frameLength;
	if(frameLength == SFBUnknownFrameLength || _startFrame >= frameLength) {
		os_log_error(gSFBAudioDecoderLog, "Invalid audio region starting frame");
		[_decoder closeReturningError:nil];
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	if(_regionFrameLength == -1)
		_endFrame = frameLength;
	else if(_regionFrameLength <= frameLength - _startFrame)
		_endFrame = _startFrame + _regionFrameLength;
	else {
		os_log_error(gSFBAudioDecoderLog, "Invalid audio region frame length");
		[_decoder closeReturningError:nil];
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	_buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_decoder.processingFormat frameCapacity:512];
	_completedLoops = 0;

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
	AVAudioFramePosition framePosition = _decoder.framePosition;
	if(framePosition == SFBUnknownFramePosition)
		return SFBUnknownFramePosition;
	AVAudioFramePosition regionFrameLength = _endFrame - _startFrame;
	return (framePosition - _startFrame) + (regionFrameLength * _completedLoops);
}

- (AVAudioFramePosition)frameLength
{
	if(_repeatCount == -1)
		return INT64_MAX;
	AVAudioFramePosition regionFrameLength = _endFrame - _startFrame;
	return regionFrameLength + (regionFrameLength * _repeatCount);
}

- (AVAudioFramePosition)regionFrameOffset
{
	AVAudioFramePosition framePosition = _decoder.framePosition;
	if(framePosition == SFBUnknownFramePosition)
		return SFBUnknownFramePosition;
	return framePosition - _startFrame;
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

	if(frameLength == 0 || (_repeatCount != -1 && _completedLoops > _repeatCount))
		return YES;

	if(frameLength > buffer.frameCapacity)
		frameLength = buffer.frameCapacity;

	AVAudioFrameCount framesRemaining = frameLength;

	while(framesRemaining > 0) {
		AVAudioFrameCount framesRemainingInRegion = (AVAudioFrameCount)(_endFrame - _decoder.framePosition);
		AVAudioFrameCount framesToDecode = MIN(MIN(framesRemaining, framesRemainingInRegion), _buffer.frameCapacity);

		// Nothing left to read
		if(framesToDecode == 0)
			break;

		// Zero the internal buffer in preparation for decoding
		_buffer.frameLength = 0;

		// Decode audio into our internal buffer and append it to output
		if(![_decoder decodeIntoBuffer:_buffer frameLength:framesToDecode error:error])
			return NO;

		[buffer appendContentsOfBuffer:_buffer];

		// Reached end of region, loop back to beginning
		if(framesToDecode == framesRemainingInRegion) {
			_completedLoops++;
			if(_repeatCount != -1 && _completedLoops <= _repeatCount) {
				if(![_decoder seekToFrame:_startFrame error:error])
					return NO;
			}
		}

		// Housekeeping
		framesRemaining -= _buffer.frameLength;
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

	if(frame >= self.frameLength) {
		if(error)
			*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil];
		return NO;
	}

	AVAudioFramePosition regionFrameLength = _endFrame - _startFrame;
	static_assert(sizeof(long long) == sizeof(AVAudioFramePosition));
	lldiv_t qr = lldiv(frame, regionFrameLength);

	if(![_decoder seekToFrame:(_startFrame + qr.rem) error:error])
		return NO;

	_completedLoops = qr.quot;
	return YES;
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ %p: %@>", [self class], self, _decoder];
}

@end
