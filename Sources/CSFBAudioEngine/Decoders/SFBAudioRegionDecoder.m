//
// Copyright (c) 2006-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

@import os.log;
@import stdlib_h;

@import AVFAudioExtensions;

#import "SFBAudioRegionDecoder.h"

#import "SFBAudioDecoder+Internal.h"

@interface SFBAudioRegionDecoder ()
{
@private
	AVAudioPCMBuffer *_buffer;
}
@end

@implementation SFBAudioRegionDecoder

@synthesize actualStartFrame = _startFrame;
@synthesize actualFrameLength = _frameLength;

- (instancetype)initWithURL:(NSURL *)url initialFrames:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithURL:url startFrame:0 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithURL:(NSURL *)url finalFrames:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithURL:url startFrame:-1 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithURL:(NSURL *)url startFrame:(AVAudioFramePosition)startFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithURL:url startFrame:startFrame frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithURL:(NSURL *)url repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	return [self initWithURL:url startFrame:0 frameLength:-1 repeatCount:repeatCount error:error];
}

- (instancetype)initWithURL:(NSURL *)url startFrame:(AVAudioFramePosition)startFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(url != nil);

	SFBInputSource *inputSource = [SFBInputSource inputSourceForURL:url flags:0 error:error];
	if(!inputSource)
		return nil;
	return [self initWithInputSource:inputSource startFrame:startFrame frameLength:frameLength repeatCount:repeatCount error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource initialFrames:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithInputSource:inputSource startFrame:0 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource finalFrames:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithInputSource:inputSource startFrame:-1 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource startFrame:(AVAudioFramePosition)startFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithInputSource:inputSource startFrame:startFrame frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	return [self initWithInputSource:inputSource startFrame:0 frameLength:-1 repeatCount:repeatCount error:error];
}

- (instancetype)initWithInputSource:(SFBInputSource *)inputSource startFrame:(AVAudioFramePosition)startFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(inputSource != nil);

	SFBAudioDecoder *decoder = [[SFBAudioDecoder alloc] initWithInputSource:inputSource error:error];
	if(!decoder)
		return nil;
	return [self initWithDecoder:decoder startFrame:startFrame frameLength:frameLength repeatCount:repeatCount error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder initialFrames:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithDecoder:decoder startFrame:0 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder finalFrames:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithDecoder:decoder startFrame:-1 frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder startFrame:(AVAudioFramePosition)startFrame frameLength:(AVAudioFramePosition)frameLength error:(NSError **)error
{
	return [self initWithDecoder:decoder startFrame:startFrame frameLength:frameLength repeatCount:0 error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	return [self initWithDecoder:decoder startFrame:0 frameLength:-1 repeatCount:repeatCount error:error];
}

- (instancetype)initWithDecoder:(id<SFBPCMDecoding>)decoder startFrame:(AVAudioFramePosition)startFrame frameLength:(AVAudioFramePosition)frameLength repeatCount:(NSInteger)repeatCount error:(NSError **)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(startFrame >= -1);
	NSParameterAssert(frameLength > 0 || frameLength == -1);
	NSParameterAssert(!(startFrame == -1 && frameLength == -1));
	NSParameterAssert(repeatCount >= -1);

	if((self = [super init])) {
		_decoder = decoder;
		_requestedStartFrame = startFrame;
		_requestedFrameLength = frameLength;
		_repeatCount = repeatCount;
		_completedLoops = 0;
		_startFrame = SFBUnknownFramePosition;
		_frameLength = SFBUnknownFramePosition;
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
		os_log_error(gSFBAudioDecoderLog, "Cannot open AudioRegionDecoder with non-seekable decoder %{public}@", _decoder);
		[_decoder closeReturningError:nil];
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	AVAudioFramePosition decoderFrameLength = _decoder.frameLength;
	if(decoderFrameLength == SFBUnknownFrameLength) {
		os_log_error(gSFBAudioDecoderLog, "Invalid frame length from %{public}@", _decoder);
		[_decoder closeReturningError:nil];
		if(error)
			*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
		return NO;
	}

	if(_requestedStartFrame == -1) {
		if(_requestedFrameLength > decoderFrameLength) {
			os_log_error(gSFBAudioDecoderLog, "Invalid requested audio region frame length %lld", _requestedFrameLength);
			[_decoder closeReturningError:nil];
			if(error)
				*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
			return NO;
		}
		_startFrame = decoderFrameLength - _requestedFrameLength;
	} else {
		if(_requestedStartFrame >= decoderFrameLength) {
			os_log_error(gSFBAudioDecoderLog, "Invalid requested audio region start frame %lld", _requestedStartFrame);
			[_decoder closeReturningError:nil];
			if(error)
				*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
			return NO;
		}
		_startFrame = _requestedStartFrame;
	}

	if(_decoder.framePosition != _startFrame && ![_decoder seekToFrame:_startFrame error:error]) {
		[_decoder closeReturningError:nil];
		return NO;
	}

	if(_requestedFrameLength == -1)
		_frameLength = decoderFrameLength - _startFrame;
	else {
		if(_requestedFrameLength > decoderFrameLength - _startFrame) {
			os_log_error(gSFBAudioDecoderLog, "Invalid requested audio region frame length %lld", _requestedFrameLength);
			[_decoder closeReturningError:nil];
			if(error)
				*error = [NSError errorWithDomain:SFBAudioDecoderErrorDomain code:SFBAudioDecoderErrorCodeInternalError userInfo:nil];
			return NO;
		}
		_frameLength = _requestedFrameLength;
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
	return (framePosition - _startFrame) + (_frameLength * _completedLoops);
}

- (AVAudioFramePosition)frameLength
{
	if(_repeatCount == -1)
		return INT64_MAX;
	return _frameLength + (_frameLength * _repeatCount);
}

- (AVAudioFramePosition)frameOffset
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
		AVAudioFrameCount framesRemainingInRegion = (AVAudioFrameCount)(_startFrame + _frameLength - _decoder.framePosition);
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
			if(_repeatCount == -1 || _completedLoops <= _repeatCount) {
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

	static_assert(sizeof(long long) == sizeof _frameLength, "AVAudioFramePosition not long long");
	lldiv_t qr = lldiv(frame, _frameLength);

	if(![_decoder seekToFrame:(_startFrame + qr.rem) error:error])
		return NO;

	_completedLoops = qr.quot;
	return YES;
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"<%@ %p: _decoder = %@, _startFrame = %lld, _frameLength = %lld, _repeatCount = %ld>", [self class], self, _decoder, _startFrame, _frameLength, (long)_repeatCount];
}

@end
