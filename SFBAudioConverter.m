/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import <AudioToolbox/AudioToolbox.h>

#import "SFBAudioConverter.h"

#import "SFBAudioBufferList+Internal.h"
#import "SFBAudioChannelLayout+Internal.h"
#import "SFBAudioDecoder+Internal.h"
#import "SFBAudioFormat+Internal.h"
#import "SFBCStringForOSType.h"

#define BUFFER_SIZE_FRAMES 512

@interface SFBAudioConverter ()
{
@private
	SFBAudioDecoder *_decoder;
	AudioConverterRef _converter;
	SFBAudioBufferList *_bufferList;
	NSError *_lastError;
}
- (OSStatus)readAudio:(AudioBufferList *)ioData frameCount:(UInt32 *)ioNumberDataPackets;
@end

// AudioConverter input callback
static OSStatus SFBAudioConverterComplexInputDataProc(AudioConverterRef				inAudioConverter,
													  UInt32						*ioNumberDataPackets,
													  AudioBufferList				*ioData,
													  AudioStreamPacketDescription	**outDataPacketDescription,
													  void							*inUserData)
{
#pragma unused(inAudioConverter)
#pragma unused(outDataPacketDescription)

	SFBAudioConverter *converter = (__bridge SFBAudioConverter *)inUserData;
	return [converter readAudio:ioData frameCount:ioNumberDataPackets];
}

@implementation SFBAudioConverter

- (instancetype)initWithDecoder:(SFBAudioDecoder *)decoder outputFormat:(SFBAudioFormat *)outputFormat
{
	return [self initWithDecoder:decoder outputFormat:outputFormat preferredBufferSize:BUFFER_SIZE_FRAMES error:nil];
}

- (instancetype)initWithDecoder:(SFBAudioDecoder *)decoder outputFormat:(SFBAudioFormat *)outputFormat error:(NSError **)error
{
	return [self initWithDecoder:decoder outputFormat:outputFormat preferredBufferSize:BUFFER_SIZE_FRAMES error:error];
}

- (instancetype)initWithDecoder:(SFBAudioDecoder *)decoder outputFormat:(SFBAudioFormat *)outputFormat preferredBufferSize:(NSInteger)preferredBufferSize error:(NSError * _Nullable __autoreleasing *)error
{
	NSParameterAssert(decoder != nil);
	NSParameterAssert(outputFormat != nil);

	// Open the decoder if necessary
	if(!decoder.isOpen && ![decoder openReturningError:error]) {
		if(error)
			os_log_error(OS_LOG_DEFAULT, "Error opening decoder: %{public}@", *error);

		return nil;
	}

	if((self = [super init])) {
		_decoder = decoder;
		_outputFormat = outputFormat;

		OSStatus result = AudioConverterNew(&_decoder->_processingFormat->_streamDescription, &_outputFormat->_streamDescription, &_converter);
		if(noErr != result) {
			os_log_error(OS_LOG_DEFAULT, "AudioConverterNew failed: %d '%{public}.4s", result, SFBCStringForOSType(result));

			if(error)
				*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];

			return nil;
		}

		// Calculate input buffer size required for preferred output buffer size
		UInt32 inputBufferSize = (UInt32)[_outputFormat frameCountToByteCount:preferredBufferSize];
		UInt32 dataSize = sizeof(inputBufferSize);
		result = AudioConverterGetProperty(_converter, kAudioConverterPropertyCalculateInputBufferSize, &dataSize, &inputBufferSize);
		if(noErr != result)
			os_log_error(OS_LOG_DEFAULT, "AudioConverterGetProperty (kAudioConverterPropertyCalculateInputBufferSize) failed: %d", result);

		UInt32 inputBufferSizeFrames = noErr == result ? (UInt32)[_decoder->_processingFormat byteCountToFrameCount:inputBufferSize] : (UInt32)preferredBufferSize;
		_bufferList = [[SFBAudioBufferList alloc] initWithFormat:_decoder->_processingFormat capacityFrames:inputBufferSizeFrames];
		if(!_bufferList) {
			os_log_error(OS_LOG_DEFAULT, "Error allocating conversion buffer");

			if(error)
				*error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];

			return nil;
		}

		// Set the channel layouts
		if(_decoder->_processingFormat->_channelLayout) {
			result = AudioConverterSetProperty(_converter, kAudioConverterInputChannelLayout, (UInt32)sizeof(_decoder->_processingFormat->_channelLayout->_layout), _decoder->_processingFormat->_channelLayout->_layout);
			if(noErr != result) {
				os_log_error(OS_LOG_DEFAULT, "AudioConverterSetProperty (kAudioConverterInputChannelLayout) failed: %d '%{public}.4s", result, SFBCStringForOSType(result));

				if(error)
					*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];

				return false;
			}
		}

		if(_outputFormat->_channelLayout) {
			result = AudioConverterSetProperty(_converter, kAudioConverterOutputChannelLayout, (UInt32)sizeof(_outputFormat->_channelLayout->_layout), _outputFormat->_channelLayout->_layout);
			if(noErr != result) {
				os_log_error(OS_LOG_DEFAULT, "AudioConverterSetProperty (kAudioConverterOutputChannelLayout) failed: %d '%{public}.4s", result, SFBCStringForOSType(result));

				if(error)
					*error = [NSError errorWithDomain:NSOSStatusErrorDomain code:result userInfo:nil];

				return false;
			}
		}
	}
	return self;
}

- (void)dealloc
{
	AudioConverterDispose(_converter);
}

- (BOOL)convertAudio:(SFBAudioBufferList *)bufferList frameCount:(NSInteger)frameCount framesConverted:(NSInteger *)framesConverted error:(NSError **)error
{
	NSParameterAssert(bufferList != nil);
	NSParameterAssert(frameCount > 0);

	UInt32 frames = (UInt32)frameCount;
	OSStatus result = AudioConverterFillComplexBuffer(_converter, SFBAudioConverterComplexInputDataProc, (__bridge void * _Nullable)(self), &frames, bufferList->_bufferList, NULL);
	if(result != noErr) {
		os_log_error(OS_LOG_DEFAULT, "AudioConverterFillComplexBuffer failed: %d '%{public}.4s", result, SFBCStringForOSType(result));
		if(error && _lastError)
			*error = _lastError;
		return NO;
	}

	*framesConverted = frames;
	return YES;
}

- (BOOL)reset
{
	OSStatus result = AudioConverterReset(_converter);
	if(result != noErr) {
		os_log_error(OS_LOG_DEFAULT, "AudioConverterReset failed: %d '%{public}.4s", result, SFBCStringForOSType(result));
		return NO;
	}

	return YES;
}

- (OSStatus)readAudio:(AudioBufferList *)ioData frameCount:(UInt32 *)ioNumberDataPackets
{
	[_bufferList reset];

	NSInteger framesRead;
	NSError *error;
	if(![_decoder decodeAudio:_bufferList frameCount:*ioNumberDataPackets framesRead:&framesRead error:&error]) {
		_lastError = error;
		*ioNumberDataPackets = 0;
		return kAudioConverterErr_UnspecifiedError;
	}

	ioData->mNumberBuffers = _bufferList->_bufferList->mNumberBuffers;
	for(UInt32 bufferIndex = 0; bufferIndex < _bufferList->_bufferList->mNumberBuffers; ++bufferIndex)
		ioData->mBuffers[bufferIndex] = _bufferList->_bufferList->mBuffers[bufferIndex];

	*ioNumberDataPackets = (UInt32)framesRead;

	return noErr;
}

@end
